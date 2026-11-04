/****************************************************************************
 * apps/examples/s31mediacheck/s31mediacheck_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <media_player.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SAMPLE_RATE 16000
#define FRAME_COUNT (SAMPLE_RATE * 3)
#define TEST_PATH "/tmp/s31-media-check.wav"

/* FLAT checks board-owned external BSS once per boot. KERNEL checks ordinary
 * user BSS in this process; it must not create an unmapped .ext_ram section.
 * A fresh process has a fresh marker and does not prove board reset behavior.
 */

#ifdef CONFIG_BUILD_KERNEL
static volatile uint32_t g_boot_marker;
#else
static volatile uint32_t g_boot_marker
  __attribute__((section(".ext_ram.bss")));
#endif

/* Exercise the OS notification transport independently of MEDIA. */

static int check_ipc(void)
{
  int mode;

  for (mode = 0; mode < 2; mode++)
    {
      int pair[2];
      int flags = SOCK_STREAM | (mode ? SOCK_NONBLOCK : 0);
      int ret = 1;
      char value = 0;

      if (socketpair(AF_UNIX, flags, 0, pair) < 0)
        {
          return 1;
        }

      if (recv(pair[0], &value, 1, MSG_DONTWAIT) != -1 ||
          errno != EAGAIN)
        {
          goto out;
        }

      if (send(pair[1], "Q", 1, 0) != 1)
        {
          goto out;
        }

      close(pair[1]);
      pair[1] = -1;
      if (recv(pair[0], &value, 1, MSG_DONTWAIT | MSG_PEEK) != 1 ||
          value != 'Q' || recv(pair[0], &value, 1, MSG_DONTWAIT) != 1 ||
          value != 'Q' || recv(pair[0], &value, 1, MSG_DONTWAIT) != 0 ||
          recv(pair[0], &value, 1, MSG_DONTWAIT | MSG_PEEK) != 0)
        {
          goto out;
        }

      ret = 0;
out:
      close(pair[0]);
      if (pair[1] >= 0)
        {
          close(pair[1]);
        }

      printf("MEDIA_CHECK local_socket mode=%d result=%d\n", mode, ret);
      if (ret != 0)
        {
          return ret;
        }
    }

  puts("MEDIA_CHECK local_socket_eof PASS");
  return 0;
}

struct playback_s
{
  pthread_mutex_t mutex;
  sem_t terminal;
  int error;
  bool completed;
};

static void put16(uint8_t *p, uint16_t value)
{
  p[0] = value;
  p[1] = value >> 8;
}

static void put32(uint8_t *p, uint32_t value)
{
  put16(p, value);
  put16(p + 2, value >> 16);
}

static int write_all(int fd, const uint8_t *p, size_t size)
{
  while (size > 0)
    {
      ssize_t count = write(fd, p, size);
      if (count < 0 && errno == EINTR)
        {
          continue;
        }

      if (count <= 0)
        {
          return count < 0 ? -errno : -EIO;
        }

      p += count;
      size -= count;
    }

  return 0;
}

static int make_wave(int fd)
{
  uint8_t header[44] = {0};
  uint8_t data[1024];
  unsigned int frame;
  int ret;

  memcpy(header, "RIFF", 4);
  put32(header + 4, 36 + FRAME_COUNT * 4);
  memcpy(header + 8, "WAVEfmt ", 8);
  put32(header + 16, 16);
  put16(header + 20, 1);
  put16(header + 22, 2);
  put32(header + 24, SAMPLE_RATE);
  put32(header + 28, SAMPLE_RATE * 4);
  put16(header + 32, 4);
  put16(header + 34, 16);
  memcpy(header + 36, "data", 4);
  put32(header + 40, FRAME_COUNT * 4);
  ret = write_all(fd, header, sizeof(header));
  if (ret < 0)
    {
      return ret;
    }

  for (frame = 0; frame < FRAME_COUNT; )
    {
      unsigned int count = FRAME_COUNT - frame;
      unsigned int i;
      if (count > sizeof(data) / 4)
        {
          count = sizeof(data) / 4;
        }

      for (i = 0; i < count; i++, frame++)
        {
          int gain = frame < 256 ? frame :
                     (FRAME_COUNT - frame < 256 ? FRAME_COUNT - frame : 256);
          int left = ((int)(frame % 32) - 16) * 48 * gain / 256;
          int right = ((int)(frame % 16) - 8) * 96 * gain / 256;
          put16(data + i * 4, (uint16_t)left);
          put16(data + i * 4 + 2, (uint16_t)right);
        }

      ret = write_all(fd, data, count * 4);
      if (ret < 0)
        {
          return ret;
        }
    }

  return 0;
}

static void on_event(void *cookie, int event, int result, const char *extra)
{
  struct playback_s *state = cookie;
  (void)extra;
  printf("MEDIA_CHECK event=%d result=%d\n", event, result);
  if (event == MEDIA_EVENT_COMPLETED || result < 0)
    {
      pthread_mutex_lock(&state->mutex);
      if (result < 0 && state->error == 0)
        {
          state->error = result;
        }

      state->completed |= event == MEDIA_EVENT_COMPLETED && result == 0;
      pthread_mutex_unlock(&state->mutex);
      sem_post(&state->terminal);
    }
}

static int play_once(unsigned int iteration, const char *path)
{
  struct playback_s state = {0};
  struct timespec deadline;
  void *player;
  int ret;
  int closing;

  ret = pthread_mutex_init(&state.mutex, NULL);
  if (ret != 0)
    {
      return -ret;
    }

  if (sem_init(&state.terminal, 0, 0) != 0)
    {
      ret = -errno;
      pthread_mutex_destroy(&state.mutex);
      return ret;
    }

  player = media_player_open(MEDIA_STREAM_MUSIC);
  ret = player ? 0 : -ENODEV;
  if (ret == 0)
    {
      ret = media_player_set_event_callback(player, &state, on_event);
    }

  if (ret == 0)
    {
      ret = media_player_prepare(player, path, NULL);
    }

  if (ret == 0)
    {
      ret = media_player_start(player);
    }

  if (ret == 0)
    {
      ret = clock_gettime(CLOCK_REALTIME, &deadline);
      if (ret < 0)
        {
          ret = -errno;
        }
      else
        {
          deadline.tv_sec += 20;
          do
            {
              ret = sem_timedwait(&state.terminal, &deadline);
            }
          while (ret < 0 && errno == EINTR);

          if (ret < 0)
            {
              ret = -errno;
            }
          else
            {
              pthread_mutex_lock(&state.mutex);
              ret = state.error ? state.error : (state.completed ? 0 : -EIO);
              pthread_mutex_unlock(&state.mutex);
            }
        }
    }

  if (player)
    {
      /* Close consumes the handle and joins its listener before stack state
       * is destroyed. The board supervisor bounds a dead RPC transport.
       */

      printf("MEDIA_CHECK closing cycle=%u wait_result=%d\n", iteration, ret);
      closing = media_player_close(player, 0);
      if (ret == 0)
        {
          ret = closing;
        }
    }

  sem_destroy(&state.terminal);
  pthread_mutex_destroy(&state.mutex);
  printf("MEDIA_CHECK cycle=%u result=%d\n", iteration, ret);
  return ret;
}

int main(int argc, char *argv[])
{
  const char *path = TEST_PATH;
  unsigned int i;
  int fd;
  int ret;

  if (argc == 2 && strcmp(argv[1], "ipc") == 0)
    {
      return check_ipc();
    }

  if (argc == 2 && strcmp(argv[1], "boot") == 0)
    {
      if (g_boot_marker != 0)
        {
#ifdef CONFIG_BUILD_KERNEL
          puts("MEDIA_CHECK process_bss_zero FAIL (run once per process)");
#else
          puts("MEDIA_CHECK external_bss_zero FAIL (run once per boot)");
#endif
          return 1;
        }

      g_boot_marker = 0x3153454d;
#ifdef CONFIG_BUILD_KERNEL
      puts("MEDIA_CHECK process_bss_zero PASS");
#else
      puts("MEDIA_CHECK external_bss_zero PASS");
#endif
      return 0;
    }

  if (argc == 2 && argv[1][0] == '/')
    {
      path = argv[1];
    }
  else if (argc != 1)
    {
      puts("Usage: s31mediacheck [boot|ipc|/new/synthetic.wav]");
      return 1;
    }

  printf("MEDIA_CHECK creating synthetic WAV\n");
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0)
    {
      printf("MEDIA_CHECK create failed errno=%d; existing file preserved\n", errno);
      return 1;
    }

  ret = make_wave(fd);
  if (close(fd) < 0 && ret == 0)
    {
      ret = -errno;
    }

  printf("MEDIA_CHECK synthetic ready result=%d\n", ret);
  for (i = 1; ret == 0 && i <= 3; i++)
    {
      ret = play_once(i, path);
    }

  if (unlink(path) < 0)
    {
      int cleanup = errno;
      printf("MEDIA_CHECK cleanup failed errno=%d\n", cleanup);
      if (ret == 0)
        {
          ret = -cleanup;
        }
    }

  printf("MEDIA_CHECK RESULT %s result=%d\n", ret == 0 ? "PASS" : "FAIL", ret);
  return ret == 0 ? 0 : 1;
}
