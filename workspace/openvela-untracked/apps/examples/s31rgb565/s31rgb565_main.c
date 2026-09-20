/****************************************************************************
 * apps/examples/s31rgb565/s31rgb565_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/video/fb.h>

#include "patterns.h"

#define RGB565_FRAME_BYTES (DEMO_PIXELS * sizeof(uint16_t))
#define RGB565_CYCLE_SECONDS 30

static void usage(const char *name)
{
  printf("Usage: %s [--device /dev/fb0] [--frames N | --cycles N]\n"
         "       %s --help\n"
         "Default: continuous 30-second dashboard/color/grid cycle.\n"
         "N must be a positive integer.\n", name, name);
}

static int positive_number(const char *text, uint32_t *value)
{
  char *end;
  unsigned long number;

  if (text[0] < '0' || text[0] > '9')
    {
      return -1;
    }

  errno = 0;
  number = strtoul(text, &end, 10);
  if (errno != 0 || *end != '\0' || number == 0 || number > UINT32_MAX)
    {
      return -1;
    }

  *value = number;
  return 0;
}

static uint64_t elapsed_ms(const struct timespec *start,
                           const struct timespec *now)
{
  int64_t seconds = (int64_t)now->tv_sec - start->tv_sec;
  int64_t nanoseconds = (int64_t)now->tv_nsec - start->tv_nsec;

  return (uint64_t)(seconds * 1000 + nanoseconds / 1000000);
}

int main(int argc, char *argv[])
{
  struct fb_videoinfo_s video = {0};
  struct fb_planeinfo_s plane = {0};
  struct fb_area_s area =
    {
      .x = 0,
      .y = 0,
      .w = DEMO_WIDTH,
      .h = DEMO_HEIGHT
    };
  struct timespec start;
  struct timespec now;
  const char *device = "/dev/fb0";
  uint32_t max_frames = 0;
  uint32_t max_cycles = 0;
  uint64_t frames = 0;
  uint64_t milliseconds;
  uint16_t *pixels = MAP_FAILED;
  unsigned int previous_mode = DEMO_MODE_COUNT;
  unsigned int seconds;
  unsigned int mode;
  int result = EXIT_FAILURE;
  int fd = -1;
  int i;

  for (i = 1; i < argc; i++)
    {
      if (strcmp(argv[i], "--help") == 0)
        {
          usage(argv[0]);
          return EXIT_SUCCESS;
        }
      else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc)
        {
          device = argv[++i];
        }
      else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc &&
               max_frames == 0 && max_cycles == 0)
        {
          if (positive_number(argv[++i], &max_frames) < 0)
            {
              usage(argv[0]);
              return EXIT_FAILURE;
            }
        }
      else if (strcmp(argv[i], "--cycles") == 0 && i + 1 < argc &&
               max_frames == 0 && max_cycles == 0)
        {
          if (positive_number(argv[++i], &max_cycles) < 0)
            {
              usage(argv[0]);
              return EXIT_FAILURE;
            }
        }
      else
        {
          usage(argv[0]);
          return EXIT_FAILURE;
        }
    }

  printf("s31rgb565: OPENVELA/NUTTX RGB565 framebuffer demo\n");
  if (!rgb565_selftest())
    {
      fprintf(stderr, "s31rgb565: RGB565_SELFTEST=FAIL\n");
      return EXIT_FAILURE;
    }

  printf("s31rgb565: RGB565_SELFTEST=PASS words=65536 "
         "byte_order=little-endian\n");

  fd = open(device, O_RDWR);
  if (fd < 0)
    {
      fprintf(stderr, "s31rgb565: open %s failed: %d\n", device, errno);
      goto out;
    }

  if (ioctl(fd, FBIOGET_VIDEOINFO, (unsigned long)(uintptr_t)&video) < 0 ||
      ioctl(fd, FBIOGET_PLANEINFO, (unsigned long)(uintptr_t)&plane) < 0)
    {
      fprintf(stderr, "s31rgb565: framebuffer information failed: %d\n",
              errno);
      goto out;
    }

  printf("s31rgb565: FB=%s format=%u %ux%u bpp=%u stride=%u "
         "bytes=%zu\n", device, video.fmt, video.xres, video.yres,
         plane.bpp, plane.stride, plane.fblen);

  if (video.fmt != FB_FMT_RGB16_565 || video.xres != DEMO_WIDTH ||
      video.yres != DEMO_HEIGHT || video.nplanes != 1 ||
      plane.bpp != 16 || plane.stride != DEMO_WIDTH * sizeof(uint16_t) ||
      plane.fblen < RGB565_FRAME_BYTES || plane.xoffset != 0 ||
      plane.yoffset != 0)
    {
      fprintf(stderr, "s31rgb565: expected single-plane 800x480 RGB565, "
              "stride=1600 and zero offset\n");
      goto out;
    }

  pixels = mmap(NULL, RGB565_FRAME_BYTES, PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_FILE, fd, 0);
  if (pixels == MAP_FAILED)
    {
      fprintf(stderr, "s31rgb565: framebuffer mmap failed: %d\n", errno);
      goto out;
    }

  if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
    {
      fprintf(stderr, "s31rgb565: monotonic clock failed: %d\n", errno);
      goto out;
    }

  for (;;)
    {
      struct timespec pause = {0, 100000000};

      if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        {
          fprintf(stderr, "s31rgb565: monotonic clock failed: %d\n", errno);
          goto out;
        }

      milliseconds = elapsed_ms(&start, &now);
      if ((max_frames != 0 && frames >= max_frames) ||
          (max_cycles != 0 && milliseconds >=
           (uint64_t)max_cycles * RGB565_CYCLE_SECONDS * 1000))
        {
          break;
        }

      seconds = (milliseconds / 1000) % RGB565_CYCLE_SECONDS;
      mode = seconds < 16 ? 0 :
             seconds < 26 ? 1 + (seconds - 16) / 2 : 6;
      if (mode != previous_mode)
        {
          printf("s31rgb565: PATTERN=%s frame=%" PRIu64 "\n",
                 rgb565_mode_name(mode), frames);
          previous_mode = mode;
        }

      rgb565_render(pixels, mode, (unsigned int)frames);

      /* The S31 framebuffer driver copies this drawing buffer to a spare
       * scanout buffer and waits for the DMA switch before returning.  A
       * completed UPDATE therefore permits the next redraw; no pan needed.
       */

      if (ioctl(fd, FBIO_UPDATE, (unsigned long)(uintptr_t)&area) < 0)
        {
          fprintf(stderr, "s31rgb565: FRAMEBUFFER_UPDATE=FAIL "
                  "frame=%" PRIu64 " errno=%d\n", frames, errno);
          goto out;
        }

      frames++;
      if (frames == 1)
        {
          printf("s31rgb565: FIRST_FRAME_UPDATE=PASS\n");
        }

      if (frames % 100 == 0)
        {
          printf("s31rgb565: LCD_FRAMES=%" PRIu64
                 " FRAMEBUFFER_UPDATE=PASS optical=unverified\n", frames);
        }

      if (max_frames != 0 && frames >= max_frames)
        {
          break;
        }

      while (nanosleep(&pause, &pause) < 0)
        {
          if (errno != EINTR)
            {
              fprintf(stderr, "s31rgb565: nanosleep failed: %d\n", errno);
              goto out;
            }
        }
    }

  printf("s31rgb565: RUN=PASS frames=%" PRIu64
         " optical=unverified\n", frames);
  result = EXIT_SUCCESS;

out:
  if (pixels != MAP_FAILED)
    {
      munmap(pixels, RGB565_FRAME_BYTES);
    }

  if (fd >= 0)
    {
      close(fd);
    }

  return result;
}
