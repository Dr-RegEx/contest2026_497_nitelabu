/****************************************************************************
 * apps/examples/s31keys/s31keys_main.c
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/input/buttons.h>

static int64_t keys_milliseconds(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    {
      return -1;
    }

  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main(int argc, char *argv[])
{
  struct btn_pollevents_s events;
  struct pollfd pfd;
  btn_buttonset_t supported;
  btn_buttonset_t state;
  int64_t now;
  int64_t deadline;
  char *end;
  long seconds = 30;
  int ret = EXIT_FAILURE;
  int fd;
  ssize_t nbytes;

  if (argc > 2 || (argc == 2 && strcmp(argv[1], "--help") == 0))
    {
      printf("Usage: s31keys [seconds: 1..300, default 30]\n");
      return argc == 2 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

  if (argc == 2)
    {
      errno = 0;
      seconds = strtol(argv[1], &end, 10);
      if (errno != 0 || end == argv[1] || *end != '\0' ||
          seconds < 1 || seconds > 300)
        {
          fprintf(stderr, "s31keys: duration must be 1..300 seconds\n");
          return EXIT_FAILURE;
        }
    }

  now = keys_milliseconds();
  if (now < 0)
    {
      perror("s31keys: clock");
      return EXIT_FAILURE;
    }

  deadline = now + seconds * 1000;
  fd = open("/dev/buttons", O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      perror("s31keys: open");
      return EXIT_FAILURE;
    }

  if (ioctl(fd, BTNIOC_SUPPORTED, (unsigned long)(uintptr_t)&supported) < 0)
    {
      perror("s31keys: supported");
      goto out;
    }

  if (supported != 0x0f)
    {
      fprintf(stderr, "s31keys: expected four Korvo keys, got 0x%lx\n",
              (unsigned long)supported);
      goto out;
    }

  events.bp_press = supported;
  events.bp_release = supported;
  if (ioctl(fd, BTNIOC_POLLEVENTS, (unsigned long)(uintptr_t)&events) < 0)
    {
      perror("s31keys: events");
      goto out;
    }

  printf("s31keys: %ld s; bits PLAY=1 SET=2 VOL-=4 VOL+=8; 0=released\n",
         seconds);
  pfd.fd = fd;
  pfd.events = POLLIN;
  pfd.revents = 0;

  for (; ; )
    {
      int remaining;
      int ready;

      now = keys_milliseconds();
      if (now < 0)
        {
          perror("s31keys: clock");
          goto out;
        }

      if (now >= deadline)
        {
          ret = EXIT_SUCCESS;
          break;
        }

      remaining = (int)(deadline - now);
      ready = poll(&pfd, 1, remaining > 1000 ? 1000 : remaining);
      if (ready < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          perror("s31keys: poll");
          goto out;
        }

      if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
          fprintf(stderr, "s31keys: device poll error 0x%lx\n",
                  (unsigned long)pfd.revents);
          goto out;
        }

      if (ready > 0 && (pfd.revents & POLLIN) != 0)
        {
          nbytes = read(fd, &state, sizeof(state));
          if (nbytes < 0 && (errno == EINTR || errno == EAGAIN))
            {
              continue;
            }

          if (nbytes != sizeof(state) || (state & ~supported) != 0)
            {
              fprintf(stderr, "s31keys: invalid sample (%ld bytes)\n",
                      (long)nbytes);
              goto out;
            }

          printf("s31keys: state=0x%lx\n", (unsigned long)state);
          fflush(stdout);
        }
    }

out:
  if (close(fd) < 0)
    {
      perror("s31keys: close");
      ret = EXIT_FAILURE;
    }

  return ret;
}
