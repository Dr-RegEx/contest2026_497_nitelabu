/****************************************************************************
 * apps/examples/s31fbcheck/s31fbcheck_main.c
 * SPDX-License-Identifier: Apache-2.0
 *
 * Standard-device qualification, synthetic pixels only. The framebuffer
 * must be exclusively reserved for this run; no GUI may draw concurrently.
 ****************************************************************************/

#define _GNU_SOURCE 1
#include <sys/ioctl.h>
#include <sys/mman.h>
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
#include <nuttx/video/fb.h>

#define WIDTH 800u
#define HEIGHT 480u
#define FRAME_BYTES (WIDTH * HEIGHT * 2u)
#define CHUNK 4093u
#define TIME_LIMIT 60

struct run_s
{
  int cpu;
  struct timespec deadline;
  unsigned cpu_checks;
  unsigned updates;
  uint64_t update_ns;
  unsigned mapped_written;
  unsigned file_written;
  unsigned mapped_verified;
  unsigned file_verified;
  uint32_t digest;
};

static int number(const char *s, unsigned limit, unsigned *value)
{
  char *end;
  if (s == NULL || *s == '\0' || *s == '-' || *s == '+') return EINVAL;
  errno = 0;
  unsigned long v = strtoul(s, &end, 10);
  if (errno || *end || v > limit) return EINVAL;
  *value = v;
  return 0;
}

static int guard(struct run_s *run)
{
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return errno;
  if (now.tv_sec > run->deadline.tv_sec ||
      (now.tv_sec == run->deadline.tv_sec &&
       now.tv_nsec >= run->deadline.tv_nsec)) return ETIMEDOUT;
  if (sched_getcpu() != run->cpu) return EXDEV;
  run->cpu_checks++;
  return 0;
}

static uint8_t pattern(unsigned seed, unsigned offset)
{
  unsigned pixel = offset / 2;
  unsigned x = pixel % WIDTH;
  unsigned y = pixel / WIDTH;
  uint16_t value = ((((x >> 3) + seed) & 31) << 11) |
                   ((((y >> 2) + seed) & 63) << 5) |
                   (((x ^ y) + seed) & 31);
  return (uint8_t)(value >> ((offset & 1) * 8));
}

static int fill_mapping(struct run_s *run, volatile uint8_t *map,
                         unsigned seed)
{
  for (unsigned y = 0; y < HEIGHT; y++)
    {
      int ret = guard(run);
      if (ret) return ret;
      for (unsigned x = 0; x < WIDTH * 2; x++)
        {
          unsigned offset = y * WIDTH * 2 + x;
          map[offset] = pattern(seed, offset);
        }
    }

  run->mapped_written += FRAME_BYTES;
  return 0;
}

static int verify_mapping(struct run_s *run, volatile const uint8_t *map,
                           unsigned seed)
{
  uint32_t hash = UINT32_C(2166136261);
  for (unsigned y = 0; y < HEIGHT; y++)
    {
      int ret = guard(run);
      if (ret) return ret;
      for (unsigned x = 0; x < WIDTH * 2; x++)
        {
          unsigned offset = y * WIDTH * 2 + x;
          uint8_t value = map[offset];
          if (value != pattern(seed, offset)) return EILSEQ;
          hash = (hash ^ value) * UINT32_C(16777619);
        }
    }

  run->digest = hash;
  run->mapped_verified += FRAME_BYTES;
  return 0;
}

static int file_transfer(struct run_s *run, int fd, unsigned seed, bool writeit)
{
  uint8_t data[CHUNK];
  unsigned offset = 0;
  uint32_t hash = UINT32_C(2166136261);
  if (lseek(fd, 0, SEEK_SET) != 0) return errno ? errno : EIO;
  while (offset < FRAME_BYTES)
    {
      int ret = guard(run);
      if (ret) return ret;
      unsigned size = FRAME_BYTES - offset;
      if (size > sizeof(data)) size = sizeof(data);
      if (writeit)
        for (unsigned i = 0; i < size; i++) data[i] = pattern(seed, offset + i);
      ssize_t n = writeit ? write(fd, data, size) : read(fd, data, size);
      if (n < 0)
        {
          if (errno == EINTR) continue;
          return errno;
        }

      if (n == 0 || (size_t)n > size) return EIO;
      if (!writeit)
        for (ssize_t i = 0; i < n; i++)
          {
            if (data[i] != pattern(seed, offset + i)) return EILSEQ;
            hash = (hash ^ data[i]) * UINT32_C(16777619);
          }
      offset += n;
    }

  if (writeit) run->file_written += offset;
  else
    {
      ssize_t n;
      do
        {
          int ret = guard(run);
          if (ret) return ret;
          n = read(fd, data, 1);
        }
      while (n < 0 && errno == EINTR);
      if (n != 0) return n < 0 ? errno : EIO;
      if (hash != run->digest) return EILSEQ;
      run->file_verified += offset;
    }

  return 0;
}

static int update(struct run_s *run, int fd, bool partial)
{
  struct fb_area_s area = {0, 0, WIDTH, HEIGHT};
  struct timespec begin;
  struct timespec end;
  int ret = guard(run);
  if (ret) return ret;
  if (partial)
    {
      area.x = 397;
      area.y = 239;
      area.w = 17;
      area.h = 13;
    }

  if (clock_gettime(CLOCK_MONOTONIC, &begin) != 0) return errno;
  if (ioctl(fd, FBIO_UPDATE, (unsigned long)(uintptr_t)&area) < 0) return errno;
  if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) return errno;
  int64_t elapsed = (int64_t)(end.tv_sec - begin.tv_sec) * INT64_C(1000000000) +
                    end.tv_nsec - begin.tv_nsec;
  if (elapsed < 0) return EIO;
  run->update_ns += elapsed;
  run->updates++;
  return guard(run);
}

static int verify_all(struct run_s *run, int fd, uint8_t *a, uint8_t *b,
                       unsigned seed)
{
  int ret = verify_mapping(run, a, seed);
  if (ret == 0) ret = verify_mapping(run, b, seed);
  if (ret == 0) ret = file_transfer(run, fd, seed, false);
  return ret;
}

static int reject_requests(int fd)
{
  const struct
  {
    size_t size;
    off_t offset;
    int prot;
    int flags;
    int error;
  } cases[] =
  {
    {FRAME_BYTES, 0, PROT_READ, MAP_SHARED, ENOTSUP},
    {FRAME_BYTES, 0, PROT_READ | PROT_WRITE, MAP_PRIVATE, ENOTSUP},
    {FRAME_BYTES, 0, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, ENOSYS},
    {FRAME_BYTES + 1, 0, PROT_READ | PROT_WRITE, MAP_SHARED, EINVAL},
    {1, FRAME_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, EINVAL},
    {0, 0, PROT_READ | PROT_WRITE, MAP_SHARED, EINVAL}
  };

  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
      errno = 0;
      void *p = mmap(NULL, cases[i].size, cases[i].prot, cases[i].flags,
                     fd, cases[i].offset);
      if (p != MAP_FAILED)
        {
          /* Unexpectedly accepted mapping is still owned by this task. */
          munmap(p, cases[i].size ? cases[i].size : 4096);
          return EPROTO;
        }

      if (errno != cases[i].error) return EPROTO;
    }

  return 0;
}

int main(int argc, char **argv)
{
  struct run_s run = {0};
  struct fb_videoinfo_s video = {0};
  struct fb_planeinfo_s plane = {0};
  uint8_t *a = MAP_FAILED;
  uint8_t *b = MAP_FAILED;
  unsigned seed;
  unsigned cpu;
  int fd = -1;
  int ret = EINVAL;
  bool painting = argc == 5 && !strcmp(argv[1], "paint");
  bool verifying = argc == 5 && !strcmp(argv[1], "verify");
  bool exit_owned = argc == 5 && !strcmp(argv[4], "exit");
  if ((!painting && !verifying) ||
      (!exit_owned && strcmp(argv[4], "unmap")) ||
      number(argv[2], 255, &seed) || number(argv[3], CPU_SETSIZE - 1, &cpu))
    {
      fprintf(stderr, "usage: s31fbcheck paint|verify SEED CPU unmap|exit\n");
      return 2;
    }

  run.cpu = cpu;
  if (clock_gettime(CLOCK_MONOTONIC, &run.deadline) != 0)
    {
      ret = errno;
      goto done;
    }
  run.deadline.tv_sec += TIME_LIMIT;
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  if (sched_setaffinity(0, sizeof(set), &set) != 0)
    {
      ret = errno;
      goto done;
    }

  ret = guard(&run);
  if (ret) goto done;
  fd = open("/dev/fb0", O_RDWR);
  if (fd < 0) { ret = errno; goto done; }
  if (ioctl(fd, FBIOGET_VIDEOINFO, (unsigned long)(uintptr_t)&video) < 0 ||
      ioctl(fd, FBIOGET_PLANEINFO, (unsigned long)(uintptr_t)&plane) < 0)
    { ret = errno; goto done; }
  if (video.fmt != FB_FMT_RGB16_565 || video.xres != WIDTH ||
      video.yres != HEIGHT || video.nplanes != 1 || plane.bpp != 16 ||
      plane.stride != WIDTH * 2 || plane.fblen != FRAME_BYTES)
    { ret = EPROTO; goto done; }
  ret = reject_requests(fd);
  if (ret) goto done;
  a = mmap(NULL, FRAME_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (a == MAP_FAILED) { ret = errno; goto done; }
  b = mmap(NULL, FRAME_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (b == MAP_FAILED) { ret = errno; goto done; }
  if (a == b) { ret = EPROTO; goto done; }
  if (painting)
    {
      for (unsigned i = 0; i < 2 && ret == 0; i++)
        {
          unsigned current = seed ^ (i == 0 ? 0x5a : 0xa5);
          ret = fill_mapping(&run, a, current);
          if (ret == 0) ret = update(&run, fd, false);
          if (ret == 0) ret = verify_all(&run, fd, a, b, current);
        }

      if (ret == 0) ret = file_transfer(&run, fd, seed, true);
      if (ret == 0) ret = update(&run, fd, false);
      if (ret == 0) ret = verify_all(&run, fd, a, b, seed);
    }

  if (ret == 0) ret = update(&run, fd, true);
  if (ret == 0) ret = verify_all(&run, fd, a, b, seed);
done:
  /* Only a successful exit-mode run deliberately leaves resources to the
   * OS. Failure always attempts every owned release, without retrying close.
   */

  if (!exit_owned || ret != 0)
    {
      if (b != MAP_FAILED && munmap(b, FRAME_BYTES) < 0 && ret == 0) ret = errno;
      if (a != MAP_FAILED && a != b && munmap(a, FRAME_BYTES) < 0 && ret == 0) ret = errno;
      if (fd >= 0 && close(fd) < 0 && ret == 0) ret = errno;
    }

  printf("S31FBCHECK %s %s error=%d cpu=%d checks=%u seed=%u "
         "updates=%u update_us=%" PRIu64 " mapped_written=%u file_written=%u "
         "mapped_verified=%u file_verified=%u digest=%08" PRIx32 " cleanup=%s\n",
         argv[1], ret == 0 ? "PASS" : "FAIL", ret, run.cpu, run.cpu_checks, seed,
         run.updates, run.update_ns / 1000, run.mapped_written, run.file_written,
         run.mapped_verified, run.file_verified, run.digest,
         exit_owned && ret == 0 ? "process-exit-required" : "explicit");
  return ret == 0 ? 0 : 1;
}
