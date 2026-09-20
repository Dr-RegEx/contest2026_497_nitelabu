/****************************************************************************
 * apps/examples/s31preview/s31preview_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/videoio.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/video/fb.h>

#define CAPTURE_WIDTH   640
#define CAPTURE_HEIGHT  480
#define DISPLAY_WIDTH   800
#define DISPLAY_HEIGHT  480
#define CAPTURE_BUFFERS 2
#define ROW_BYTES       (CAPTURE_WIDTH * 2)

static void usage(const char *name)
{
  printf("Usage: %s [--video /dev/video0] [--fb /dev/fb0]\n"
         "       [--frames N] [--timeout-ms N]\n"
         "Default: 60 real camera frames, 3000 ms timeout per frame.\n",
         name);
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
  if (errno != 0 || *end != '\0' || number == 0 || number > INT_MAX)
    {
      return -1;
    }

  *value = number;
  return 0;
}

static int monotonic_ms(uint64_t *value)
{
  struct timespec now;

  if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
    {
      return -1;
    }

  *value = (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
  return 0;
}

static int checked_ioctl(int fd, int command, void *arg, const char *name)
{
  int ret = ioctl(fd, command, (unsigned long)(uintptr_t)arg);

  if (ret < 0)
    {
      fprintf(stderr, "s31preview: %s failed errno=%d\n", name, errno);
    }

  return ret;
}

static int queue_buffer(int fd, unsigned int index, uint8_t *data,
                        size_t length)
{
  struct v4l2_buffer buffer = {0};

  buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buffer.memory = V4L2_MEMORY_USERPTR;
  buffer.index = index;
  buffer.m.userptr = (unsigned long)(uintptr_t)data;
  buffer.length = length;
  return checked_ioctl(fd, VIDIOC_QBUF, &buffer, "VIDIOC_QBUF");
}

static int dequeue_frame(int fd, uint32_t timeout_ms,
                         struct v4l2_buffer *buffer)
{
  struct pollfd pfd = {.fd = fd, .events = POLLIN};
  uint64_t start;
  uint64_t now;
  int ret;

  if (monotonic_ms(&start) < 0)
    {
      return -1;
    }

  for (;;)
    {
      if (monotonic_ms(&now) < 0)
        {
          return -1;
        }

      if (now - start >= timeout_ms)
        {
          errno = ETIMEDOUT;
          return -1;
        }

      pfd.revents = 0;
      ret = poll(&pfd, 1, (int)(timeout_ms - (now - start)));
      if (ret < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          return -1;
        }

      if (ret == 0)
        {
          errno = ETIMEDOUT;
          return -1;
        }

      if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
        {
          errno = EIO;
          return -1;
        }

      if ((pfd.revents & POLLIN) == 0)
        {
          continue;
        }

      memset(buffer, 0, sizeof(*buffer));
      buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buffer->memory = V4L2_MEMORY_USERPTR;
      ret = ioctl(fd, VIDIOC_DQBUF, (unsigned long)(uintptr_t)buffer);
      if (ret >= 0)
        {
          return 0;
        }

      /* The capture framework can notify before completing the queue
       * transition. O_NONBLOCK keeps this race inside the same deadline.
       */

      if (errno != EAGAIN && errno != EINTR)
        {
          return -1;
        }
    }
}

static uint32_t copy_frame(uint8_t *display, size_t display_stride,
                           const uint8_t *capture, size_t capture_stride)
{
  uint32_t hash = UINT32_C(2166136261);
  unsigned int row;
  unsigned int column;

  for (row = 0; row < CAPTURE_HEIGHT; row++)
    {
      const uint8_t *source = capture + row * capture_stride;
      uint8_t *dest = display + row * display_stride +
                     (DISPLAY_WIDTH - CAPTURE_WIDTH);

      memcpy(dest, source, ROW_BYTES);
      for (column = 0; column < ROW_BYTES; column++)
        {
          hash = (hash ^ source[column]) * UINT32_C(16777619);
        }
    }

  return hash;
}

int main(int argc, char *argv[])
{
  struct fb_videoinfo_s video = {0};
  struct fb_planeinfo_s plane = {0};
  struct fb_area_s area =
    {.x = 0, .y = 0, .w = DISPLAY_WIDTH, .h = DISPLAY_HEIGHT};
  struct v4l2_format format = {0};
  struct v4l2_requestbuffers request = {0};
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  const char *video_path = "/dev/video0";
  const char *fb_path = "/dev/fb0";
  uint8_t *capture[CAPTURE_BUFFERS] = {NULL};
  uint8_t *display = MAP_FAILED;
  uint32_t max_frames = 60;
  uint32_t timeout_ms = 3000;
  uint32_t frames = 0;
  uint64_t start = 0;
  size_t capture_stride;
  size_t capture_size;
  size_t required;
  size_t display_size = 0;
  unsigned int count = 0;
  unsigned int index;
  bool requested = false;
  bool streaming = false;
  int video_fd = -1;
  int fb_fd = -1;
  int result = EXIT_FAILURE;
  int i;

  for (i = 1; i < argc; i++)
    {
      if (strcmp(argv[i], "--help") == 0)
        {
          usage(argv[0]);
          return EXIT_SUCCESS;
        }
      else if (strcmp(argv[i], "--video") == 0 && i + 1 < argc)
        {
          video_path = argv[++i];
        }
      else if (strcmp(argv[i], "--fb") == 0 && i + 1 < argc)
        {
          fb_path = argv[++i];
        }
      else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
        {
          if (positive_number(argv[++i], &max_frames) < 0)
            {
              usage(argv[0]);
              return EXIT_FAILURE;
            }
        }
      else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc)
        {
          if (positive_number(argv[++i], &timeout_ms) < 0)
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

  fb_fd = open(fb_path, O_RDWR);
  if (fb_fd < 0)
    {
      fprintf(stderr, "s31preview: open %s failed errno=%d\n", fb_path,
              errno);
      goto out;
    }

  if (checked_ioctl(fb_fd, FBIOGET_VIDEOINFO, &video,
                    "FBIOGET_VIDEOINFO") < 0 ||
      checked_ioctl(fb_fd, FBIOGET_PLANEINFO, &plane,
                    "FBIOGET_PLANEINFO") < 0)
    {
      goto out;
    }

  if (video.fmt != FB_FMT_RGB16_565 || video.nplanes != 1 ||
      video.xres != DISPLAY_WIDTH || video.yres != DISPLAY_HEIGHT ||
      plane.bpp != 16 || plane.stride < DISPLAY_WIDTH * 2 ||
      plane.xoffset != 0 || plane.yoffset != 0)
    {
      fprintf(stderr, "s31preview: incompatible framebuffer layout\n");
      goto out;
    }

  display_size = (size_t)plane.stride * DISPLAY_HEIGHT;
  if (plane.fblen < display_size)
    {
      fprintf(stderr, "s31preview: framebuffer storage too short\n");
      goto out;
    }

#ifdef CONFIG_BUILD_FLAT
  /* FLAT fb_mmap returns the driver address without registering a mapping.
   * Use that same address directly; there is no mapping to munmap. */
  display = plane.fbmem;
#else
  display = mmap(NULL, display_size, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FILE, fb_fd, 0);
#endif
  if (display == MAP_FAILED)
    {
      fprintf(stderr, "s31preview: framebuffer mmap failed errno=%d\n",
              errno);
      goto out;
    }

  /* The margins stay black; no framebuffer update occurs until a real
   * captured frame has been validated and copied into the center.
   */

  memset(display, 0, display_size);
  video_fd = open(video_path, O_RDWR | O_NONBLOCK);
  if (video_fd < 0)
    {
      fprintf(stderr, "s31preview: open %s failed errno=%d\n", video_path,
              errno);
      goto out;
    }

  format.type = type;
  format.fmt.pix.width = CAPTURE_WIDTH;
  format.fmt.pix.height = CAPTURE_HEIGHT;
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  format.fmt.pix.field = V4L2_FIELD_ANY;
  if (checked_ioctl(video_fd, VIDIOC_S_FMT, &format, "VIDIOC_S_FMT") < 0 ||
      checked_ioctl(video_fd, VIDIOC_G_FMT, &format, "VIDIOC_G_FMT") < 0)
    {
      goto out;
    }

  if (format.fmt.pix.width != CAPTURE_WIDTH ||
      format.fmt.pix.height != CAPTURE_HEIGHT ||
      format.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565 ||
      (format.fmt.pix.field != V4L2_FIELD_ANY &&
       format.fmt.pix.field != V4L2_FIELD_NONE))
    {
      fprintf(stderr, "s31preview: expected 640x480 RGB565 capture\n");
      goto out;
    }

  /* The S31 bridge copies packed RGB565 out of its own DMA buffers. The
   * capture framework can report zero bytesperline/sizeimage for this
   * fixed format. USERPTR is ordinary writable memory, not a DMA target.
   */

  capture_stride = format.fmt.pix.bytesperline != 0 ?
                   format.fmt.pix.bytesperline : ROW_BYTES;
  if (capture_stride < ROW_BYTES ||
      capture_stride > UINT32_MAX / CAPTURE_HEIGHT)
    {
      fprintf(stderr, "s31preview: invalid capture stride\n");
      goto out;
    }

  required = (CAPTURE_HEIGHT - 1) * capture_stride + ROW_BYTES;
  capture_size = format.fmt.pix.sizeimage != 0 ?
                 format.fmt.pix.sizeimage : capture_stride * CAPTURE_HEIGHT;
  if (capture_size < required)
    {
      fprintf(stderr, "s31preview: invalid capture size\n");
      goto out;
    }

  request.type = type;
  request.memory = V4L2_MEMORY_USERPTR;
  request.mode = V4L2_BUF_MODE_FIFO;
  request.count = CAPTURE_BUFFERS;
  if (checked_ioctl(video_fd, VIDIOC_REQBUFS, &request,
                    "VIDIOC_REQBUFS") < 0)
    {
      goto out;
    }

  requested = true;
  if (request.count == 0 || request.count > CAPTURE_BUFFERS)
    {
      fprintf(stderr, "s31preview: unsupported buffer count=%" PRIu32 "\n",
              request.count);
      goto out;
    }

  count = request.count;
  for (index = 0; index < count; index++)
    {
      capture[index] = malloc(capture_size);
      if (capture[index] == NULL)
        {
          fprintf(stderr, "s31preview: capture allocation failed\n");
          goto out;
        }

      if (queue_buffer(video_fd, index, capture[index], capture_size) < 0)
        {
          goto out;
        }
    }

  printf("s31preview: video=%s 640x480 RGB565 stride=%zu bytes=%zu "
         "buffers=%u fb=%s 800x480 stride=%u x=80 frames=%" PRIu32
         " timeout_ms=%" PRIu32 "\n", video_path, capture_stride,
         capture_size, count, fb_path, plane.stride, max_frames, timeout_ms);

  streaming = true;
  if (checked_ioctl(video_fd, VIDIOC_STREAMON, &type,
                    "VIDIOC_STREAMON") < 0 || monotonic_ms(&start) < 0)
    {
      goto out;
    }

  while (frames < max_frames)
    {
      struct v4l2_buffer buffer;
      uint64_t now;
      uint32_t hash;

      if (dequeue_frame(video_fd, timeout_ms, &buffer) < 0)
        {
          fprintf(stderr, "s31preview: capture failed frame=%" PRIu32
                  " errno=%d\n", frames + 1, errno);
          goto out;
        }

      if (buffer.index >= count || buffer.type != type ||
          buffer.memory != V4L2_MEMORY_USERPTR ||
          buffer.m.userptr != (unsigned long)(uintptr_t)capture[buffer.index] ||
          buffer.length > capture_size || buffer.bytesused > buffer.length ||
          buffer.bytesused < required ||
          (buffer.flags & V4L2_BUF_FLAG_ERROR) != 0)
        {
          fprintf(stderr, "s31preview: invalid capture frame=%" PRIu32
                  " index=%" PRIu32 " bytesused=%" PRIu32
                  " length=%" PRIu32 " flags=0x%" PRIx32 "\n",
                  frames + 1, buffer.index, buffer.bytesused, buffer.length,
                  buffer.flags);
          goto out;
        }

      hash = copy_frame(display, plane.stride, capture[buffer.index],
                         capture_stride);

      /* S31 FBIO_UPDATE copies to a spare scanout buffer and waits for the
       * DMA switch. Only after it returns may the mapped drawing be reused.
       */

      if (checked_ioctl(fb_fd, FBIO_UPDATE, &area, "FBIO_UPDATE") < 0 ||
          monotonic_ms(&now) < 0)
        {
          goto out;
        }

      frames++;
      printf("s31preview: frame=%" PRIu32 " buffer=%" PRIu32
             " capture_time=%" PRIdMAX ".%06" PRIdMAX
             " elapsed_ms=%" PRIu64 " bytesused=%" PRIu32
             " fnv1a32=%08" PRIx32 " update=OK\n", frames, buffer.index,
             (intmax_t)buffer.timestamp.tv_sec,
             (intmax_t)buffer.timestamp.tv_usec, now - start,
             buffer.bytesused, hash);

      if (frames < max_frames &&
          queue_buffer(video_fd, buffer.index, capture[buffer.index],
                        capture_size) < 0)
        {
          goto out;
        }
    }

  result = EXIT_SUCCESS;

out:
  /* Stop and close the camera before releasing USERPTR storage. The bridge
   * owns DMA buffers and drains its deferred copy before close completes.
   */

  if (video_fd >= 0)
    {
      if (streaming &&
          checked_ioctl(video_fd, VIDIOC_STREAMOFF, &type,
                        "VIDIOC_STREAMOFF") < 0)
        {
          result = EXIT_FAILURE;
        }

      if (requested)
        {
          request.count = 0;
          if (checked_ioctl(video_fd, VIDIOC_REQBUFS, &request,
                            "VIDIOC_REQBUFS release") < 0)
            {
              result = EXIT_FAILURE;
            }
        }

      if (close(video_fd) < 0)
        {
          fprintf(stderr, "s31preview: video close failed errno=%d\n", errno);
          result = EXIT_FAILURE;
        }
    }

  for (index = 0; index < CAPTURE_BUFFERS; index++)
    {
      free(capture[index]);
    }

#ifndef CONFIG_BUILD_FLAT
  if (display != MAP_FAILED && munmap(display, display_size) < 0)
    {
      fprintf(stderr, "s31preview: framebuffer munmap failed errno=%d\n",
              errno);
      result = EXIT_FAILURE;
    }

#endif

  if (fb_fd >= 0 && close(fb_fd) < 0)
    {
      fprintf(stderr, "s31preview: framebuffer close failed errno=%d\n",
              errno);
      result = EXIT_FAILURE;
    }

  printf("s31preview: completed=%" PRIu32 "/%" PRIu32 " result=%s\n",
         frames, max_frames, result == EXIT_SUCCESS ? "PASS" : "FAIL");
  return result;
}
