/* SPDX-License-Identifier: Apache-2.0
 * Standard ALSA playback/lifecycle qualification; generated PCM only.
 * EXIT_ARMED is a pre-exit marker. The external runner must verify process
 * disappearance and subsequent reopening before accepting cleanup.
 */
#define _GNU_SOURCE 1
#include <alsa/asoundlib.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PERIOD_FRAMES 1024u
#define RING_FRAMES 4096u
#define PLAY_FRAMES 24017u
#define FRAME_BYTES 4u
#define TIME_LIMIT 60u

struct run_s
{
  unsigned cpu;
  unsigned seed;
  unsigned written;
  unsigned checks;
  unsigned waits;
  unsigned pauses;
  uint32_t digest;
  uint64_t deadline;
};

static uint8_t g_samples[PERIOD_FRAMES * FRAME_BYTES];

static int number(const char *text, unsigned maximum, unsigned *value)
{
  unsigned n = 0;
  if (!text || !*text) return -EINVAL;
  for (; *text; text++)
    {
      if (*text < '0' || *text > '9' || n > maximum / 10) return -EINVAL;
      n = n * 10 + (*text - '0');
      if (n > maximum) return -EINVAL;
    }
  *value = n;
  return 0;
}

static int now(uint64_t *value)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) return -errno;
  *value = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  return 0;
}

static int guard(struct run_s *run)
{
  uint64_t value;
  int ret = now(&value);
  if (ret < 0) return ret;
  if (value >= run->deadline) return -ETIMEDOUT;
  if (sched_getcpu() != (int)run->cpu) return -EXDEV;
  run->checks++;
  return 0;
}

static int delay(struct run_s *run, unsigned ms)
{
  struct timespec pause = {ms / 1000, (ms % 1000) * 1000000};
  while (nanosleep(&pause, &pause) < 0)
    {
      if (errno != EINTR) return -errno;
      int ret = guard(run);
      if (ret < 0) return ret;
    }
  return guard(run);
}

static void fill(unsigned first, unsigned frames, unsigned seed)
{
  for (unsigned i = 0; i < frames; i++)
    {
      int value = (int)(((first + i) * (seed * 2u + 1u) + 73u) & 511u) - 256;
      uint16_t left = (uint16_t)value;
      uint16_t right = (uint16_t)-value;
      g_samples[i * 4] = left;
      g_samples[i * 4 + 1] = left >> 8;
      g_samples[i * 4 + 2] = right;
      g_samples[i * 4 + 3] = right >> 8;
    }
}

static int configure(snd_pcm_t *pcm)
{
  snd_pcm_hw_params_t params;
  snd_pcm_sw_params_t sw;
  snd_pcm_uframes_t buffer;
  snd_pcm_uframes_t period;
  int ret = snd_pcm_hw_params_any(pcm, &params);
#define SET(call) do { if (ret == 0) ret = (call); } while (0)
  SET(snd_pcm_hw_params_set_access(pcm, &params, SND_PCM_ACCESS_RW_INTERLEAVED));
  SET(snd_pcm_hw_params_set_format(pcm, &params, SND_PCM_FORMAT_S16_LE));
  SET(snd_pcm_hw_params_set_channels(pcm, &params, 2));
  SET(snd_pcm_hw_params_set_rate(pcm, &params, 16000, 0));
  SET(snd_pcm_hw_params_set_period_size(pcm, &params, PERIOD_FRAMES, 0));
  SET(snd_pcm_hw_params_set_periods(pcm, &params, 4, 0));
  SET(snd_pcm_hw_params(pcm, &params));
  SET(snd_pcm_get_params(pcm, &buffer, &period));
  /* Use explicit START so the qualification observes its exact result.
   * A threshold above the ring disables the writei automatic start path.
   */
  SET(snd_pcm_sw_params_current(pcm, &sw));
  SET(snd_pcm_sw_params_set_start_threshold(pcm, &sw, RING_FRAMES + PERIOD_FRAMES));
  SET(snd_pcm_sw_params(pcm, &sw));
#undef SET
  if (ret < 0) return ret;
  if (buffer != RING_FRAMES || period != PERIOD_FRAMES ||
      snd_pcm_frames_to_bytes(pcm, 1) != FRAME_BYTES ||
      snd_pcm_state(pcm) != SND_PCM_STATE_PREPARED) return -EPROTO;
  return 0;
}

static void report(const struct run_s *run, const char *mode,
                   const char *phase, int result)
{
  printf("S31PCM mode=%s phase=%s cpu=%u seed=%u frames=%u bytes=%u "
         "hash=%08" PRIx32 " pauses=%u checks=%u waits=%u result=%d\n",
         mode, phase, run->cpu, run->seed, run->written,
         run->written * FRAME_BYTES, run->digest, run->pauses,
         run->checks, run->waits, result);
  fflush(stdout);
}

int main(int argc, char **argv)
{
  struct run_s run = {.digest = UINT32_C(2166136261)};
  snd_pcm_t *pcm = NULL;
  const char *mode = argc == 4 ? argv[1] : "invalid";
  bool play = !strcmp(mode, "play");
  bool pause = !strcmp(mode, "pause");
  bool prepared_exit = !strcmp(mode, "prepared-exit");
  bool playing_exit = !strcmp(mode, "playing-exit");
  bool paused_exit = !strcmp(mode, "paused-exit");
  bool exit_owned = prepared_exit || playing_exit || paused_exit;
  bool started = false;
  int ret;

  if (argc != 4 || (!play && !pause && !exit_owned) ||
      number(argv[2], 255, &run.seed) ||
      number(argv[3], CPU_SETSIZE - 1, &run.cpu))
    {
      fprintf(stderr, "usage: s31pcmcheck play|pause|prepared-exit|playing-exit|paused-exit SEED CPU\n");
      return 2;
    }

  ret = now(&run.deadline);
  if (ret < 0) goto done;
  run.deadline += TIME_LIMIT * 1000;
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(run.cpu, &set);
  if (sched_setaffinity(0, sizeof(set), &set) < 0)
    {
      ret = -errno;
      goto done;
    }

  if ((ret = guard(&run)) < 0) goto done;
  ret = snd_pcm_open(&pcm, "pcm0p", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
  if (ret < 0) goto done;
  if ((ret = configure(pcm)) < 0) goto done;
  report(&run, mode, "READY", 0);

  unsigned target = prepared_exit ? PERIOD_FRAMES :
                    exit_owned ? RING_FRAMES : PLAY_FRAMES;
  while (run.written < target)
    {
      if ((ret = guard(&run)) < 0) goto done;
      unsigned frames = target - run.written;
      if (frames > PERIOD_FRAMES) frames = PERIOD_FRAMES;
      fill(run.written, frames, run.seed);
      snd_pcm_sframes_t n = snd_pcm_writei(pcm, g_samples, frames);
      if (n == -EAGAIN || n == -EINTR)
        {
          run.waits++;
          if ((ret = delay(&run, 5)) < 0) goto done;
          continue;
        }
      if (n <= 0 || n > (snd_pcm_sframes_t)frames)
        {
          ret = n < 0 ? (int)n : -EIO;
          goto done;
        }
      for (unsigned i = 0; i < (unsigned)n * FRAME_BYTES; i++)
        run.digest = (run.digest ^ g_samples[i]) * UINT32_C(16777619);
      run.written += n;
      if (!started && run.written >= RING_FRAMES)
        {
          if ((ret = snd_pcm_start(pcm)) < 0) goto done;
          started = true;
        }
      if (snd_pcm_state(pcm) == SND_PCM_STATE_XRUN)
        {
          ret = -EPIPE;
          goto done;
        }
      if ((pause || paused_exit) && !run.pauses && run.written >= RING_FRAMES)
        {
          if ((ret = snd_pcm_pause(pcm, 1)) < 0) goto done;
          run.pauses++;
          if ((ret = delay(&run, 200)) < 0) goto done;
          if (snd_pcm_state(pcm) != SND_PCM_STATE_PAUSED)
            {
              ret = -EPROTO;
              goto done;
            }
          if (!paused_exit && (ret = snd_pcm_pause(pcm, 0)) < 0) goto done;
        }
    }

  if (exit_owned)
    {
      if (playing_exit && (ret = delay(&run, 20)) < 0) goto done;
      snd_pcm_state_t expected = prepared_exit ? SND_PCM_STATE_PREPARED :
                                 paused_exit ? SND_PCM_STATE_PAUSED : SND_PCM_STATE_RUNNING;
      if (snd_pcm_state(pcm) != expected)
        {
          ret = -EPROTO;
          goto done;
        }
      if ((ret = guard(&run)) < 0) goto done;
      report(&run, mode, "EXIT_ARMED", 0);
      _exit(0);
    }

  do
    {
      if ((ret = guard(&run)) < 0) goto done;
      ret = snd_pcm_drain(pcm);
      if (ret == -EAGAIN || ret == -EINTR)
        {
          run.waits++;
          if ((ret = delay(&run, 5)) < 0) goto done;
          ret = -EAGAIN;
        }
    }
  while (ret == -EAGAIN);
  if (ret < 0) goto done;
  if (snd_pcm_state(pcm) != SND_PCM_STATE_SETUP)
    {
      ret = -EPROTO;
      goto done;
    }
  ret = guard(&run);

done:
  if (pcm != NULL)
    {
      int close_ret = snd_pcm_close(pcm);
      if (ret == 0) ret = close_ret;
    }
  report(&run, mode, ret == 0 ? "CLOSED" : "FAILED", ret);
  return ret < 0 ? 1 : 0;
}
