/****************************************************************************
 * apps/examples/s31preview/s31preview_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#ifdef CONFIG_INPUT_TOUCHSCREEN
#include <nuttx/input/touchscreen.h>
#endif

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

#include "s31record.h"

static void usage(const char *name)
{
  printf("Usage: %s [--video /dev/video0] [--fb /dev/fb0]\n"
         "       [--frames N] [--timeout-ms N] [--touch] [--ui] [--record /mnt/sdcard/NAME.y4m]\n"
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

static uint32_t copy_frame_pixels(uint8_t *display, size_t display_stride,
                                   const uint8_t *capture, size_t capture_stride)
{
  for(unsigned row=0;row<CAPTURE_HEIGHT;row++) {
    uint8_t *d=display+row*display_stride+(DISPLAY_WIDTH-CAPTURE_WIDTH);
    const uint8_t *s=capture+row*capture_stride;
    if((((uintptr_t)d|(uintptr_t)s)&3)==0) {
      volatile uint32_t *dw=(volatile uint32_t *)d;
      const uint32_t *sw=(const uint32_t *)s;
      for(unsigned i=0;i<ROW_BYTES/4;i++)dw[i]=sw[i];
    } else memcpy(d,s,ROW_BYTES);
  }
  return 0;
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

#ifdef CONFIG_INPUT_TOUCHSCREEN
#include "s31ui.h"

/* Synthetic image benchmark: never starts the camera; deletes only its
 * own successfully created test file after exact readback verification. */
static int photo_benchmark(void)
{
  struct s31ui_s u={.fd=-1};bool mounted=false;int result=1;
  uint8_t *rgb=malloc(614400);uint64_t begin=0,end=0;
  char created[64]={0};
  u.pixels=malloc(614400);u.scratch=malloc(460800);
  if(!rgb||!u.pixels||!u.scratch)goto finish;
  if(!ui_sd()){if(mount("/dev/mmcsd0","/mnt/sdcard","vfat",0,NULL)<0)goto finish;mounted=true;}
  for(int y=0;y<480;y++)for(int x=0;x<640;x++)
    ((uint16_t *)rgb)[y*640+x]=y<240?(x<320?0xf800:0x07e0):(x<320?0x001f:0xffff);
  monotonic_ms(&begin);
  if(ui_photo(&u,rgb,1280)<0)goto finish;
  monotonic_ms(&end);strcpy(created,u.path);
  printf("photo-benchmark: synthetic save+fsync+close=%llu ms bytes=921654\n",(unsigned long long)(end-begin));
  strcpy(u.names[0],strrchr(created,'/')+1);ui_view(&u,0,0);
  if(u.mode==2 && memcmp(rgb,u.pixels,614400)==0)result=0;
finish:
  if(u.fd>=0)close(u.fd);
  if(created[0] && unlink(created)<0)result=1;
  if(mounted && umount("/mnt/sdcard")<0)result=1;
  free(rgb);free(u.pixels);free(u.scratch);
  printf("photo-benchmark: exact readback and test-file cleanup %s\n",result?"FAIL":"PASS");
  return result;
}

struct photo_display_s { int fd; uint8_t *fb; size_t stride; uint64_t start; };
static void photo_progress(void *arg, unsigned percent)
{
  struct photo_display_s *d=arg;
  struct fb_area_s area={.x=0,.y=0,.w=800,.h=480};
  char message[64];
  if(percent==0)monotonic_ms(&d->start);
  if(percent==99 || percent==100) {
    uint64_t now=0;monotonic_ms(&now);
    printf("camera-ui: photo %s elapsed_ms=%llu bytes=921654\n",percent==99?"write-done":"synced",(unsigned long long)(now-d->start));
    if(percent==100)return;
  }
  snprintf(message,sizeof(message),percent==99 ? "SYNCING PHOTO - PLEASE WAIT" : "SAVING PHOTO %u%% - PLEASE WAIT",percent);
  ui_rect(d->fb,d->stride,0,0,800,42,0x0320);
  ui_text(d->fb,d->stride,12,8,message);
  ui_rect(d->fb,d->stride,0,420,800,60,0x1082);
  ui_text(d->fb,d->stride,20,436,"SAVING - DO NOT REMOVE SD CARD");
  ioctl(d->fd,FBIO_UPDATE,(unsigned long)&area);
}
#endif

int main(int argc, char *argv[])
{
#ifdef CONFIG_INPUT_TOUCHSCREEN
  if(argc==2 && strcmp(argv[1],"--photo-bench")==0)return photo_benchmark();
#endif
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
  struct s31record_s recording = {.fd = -1};
  const char *record_path = NULL;
  uint32_t max_frames = 60;
  bool touch = false;
  bool profile = false;
  uint64_t stages[5]={0}, mark=0, stamp=0;
#ifdef CONFIG_INPUT_TOUCHSCREEN
  bool ui_mode = false;
  bool return_home = false;
  bool standby_test = false;
  struct s31ui_s ui = {.fd = -1};
  struct photo_display_s photo_display={0};
#endif
  int touch_fd = -1;
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
      else if (strcmp(argv[i], "--profile") == 0)
        { profile = true; }
      else if (strcmp(argv[i], "--record") == 0 && i + 1 < argc)
        {
          record_path = argv[++i];
        }
#ifdef CONFIG_INPUT_TOUCHSCREEN
      else if (strcmp(argv[i], "--standby-test") == 0)
        { standby_test = true; ui_mode = true; touch = true; max_frames = 40; }
      else if (strcmp(argv[i], "--return-home") == 0)
        { return_home = true; }
      else if (strcmp(argv[i], "--ui") == 0)
        { ui_mode = true; touch = true; max_frames = UINT32_MAX; }
#endif
      else if (strcmp(argv[i], "--touch") == 0)
        {
          touch = true;
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

#ifdef CONFIG_INPUT_TOUCHSCREEN
  if (ui_mode)
    {
      if (record_path) { fprintf(stderr, "--ui cannot auto-record\n"); goto out; }
      ui.pixels = malloc(640 * 480 * 2);
      ui.scratch = malloc(460800);
      if (!ui.pixels || !ui.scratch) goto out;
      if (!ui_sd() && mount("/dev/mmcsd0", "/mnt/sdcard", "vfat", 0, NULL) == 0)
        ui.mounted = true;
      snprintf(ui.status,sizeof(ui.status), ui_sd() ? "LIVE - READY" : "LIVE - NO SD CARD");
    }
#endif
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

#ifdef CONFIG_INPUT_TOUCHSCREEN
  if(ui_mode) {
    photo_display.fd=fb_fd;photo_display.fb=display;photo_display.stride=plane.stride;
    ui.photo_progress=photo_progress;ui.photo_progress_arg=&photo_display;
  }
#endif

  /* The margins stay black; no framebuffer update occurs until a real
   * captured frame has been validated and copied into the center.
   */

  if (record_path && record_open(&recording, record_path) < 0)
    { perror("s31preview record open"); goto out; }

  memset(display, 0, display_size);
  if (touch)
    {
      touch_fd = open("/dev/input0", O_RDONLY | O_NONBLOCK);
      if (touch_fd < 0) { perror("s31preview touch"); goto out; }
    }
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

      if(profile)monotonic_ms(&mark);
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

      if(profile){monotonic_ms(&stamp);stages[0]+=stamp-mark;mark=stamp;}
      if (recording.fd >= 0 &&
          record_frame(&recording, capture[buffer.index], capture_stride, &buffer.timestamp) < 0)
        { perror("s31preview record frame"); goto out; }

      if(profile){monotonic_ms(&stamp);stages[1]+=stamp-mark;mark=stamp;}
#ifdef CONFIG_INPUT_TOUCHSCREEN
      if(ui_mode)hash=copy_frame_pixels(display,plane.stride,capture[buffer.index],capture_stride);
      else
#endif
      hash = copy_frame(display, plane.stride, capture[buffer.index],
                         capture_stride);

      if(profile){monotonic_ms(&stamp);stages[2]+=stamp-mark;mark=stamp;}
#ifdef CONFIG_INPUT_TOUCHSCREEN
      if (ui_mode)
        {
          monotonic_ms(&now);
          if (!standby_test)
            ui_step(&ui, &recording, display, plane.stride,
                  capture[buffer.index], capture_stride, touch_fd, now);
          if (standby_test && frames > 0 && frames % 10 == 0)
            ui.quit = true;
          if (ui.quit && return_home)
            break;
          if (ui.quit)
            {
              /* This capture backend retains its queue on STREAMOFF and
               * REQBUFS(0) is a no-op. Return only the dequeued frame; the
               * other buffer is still owned by the queue. */
              if (checked_ioctl(video_fd, VIDIOC_STREAMOFF, &type,
                                "standby STREAMOFF") < 0) goto out;
              streaming = false;
              if (queue_buffer(video_fd, buffer.index, capture[buffer.index],
                               capture_size) < 0) goto out;
              ui_rect(display, plane.stride, 0, 0, 800, 480, 0x1082);
              ui_text(display, plane.stride, 280, 145, "STANDBY");
              ui_rect(display, plane.stride, 240, 240, 320, 90, 0x0320);
              ui_text(display, plane.stride, 312, 272, "OPEN CAMERA");
              if (checked_ioctl(fb_fd, FBIO_UPDATE, &area,
                                "standby display") < 0) goto out;
              printf("camera-ui: STANDBY capture stopped; no recording\n");
              bool reopen = standby_test;
              if (standby_test) usleep(200000);
              while (!reopen)
                {
                  struct touch_sample_s sample;
                  while (read(touch_fd, &sample, sizeof(sample)) == sizeof(sample))
                    if (sample.npoints && (sample.point[0].flags & TOUCH_DOWN) &&
                        sample.point[0].x >= 240 && sample.point[0].x < 560 &&
                        sample.point[0].y >= 240 && sample.point[0].y < 330)
                      reopen = true;
                  if (!reopen) usleep(20000);
                }
              streaming = true;
              if (checked_ioctl(video_fd, VIDIOC_STREAMON, &type,
                                "resume STREAMON") < 0) goto out;
              ui.quit = false;
              ui.confirm = 0;
              snprintf(ui.status, sizeof(ui.status), "LIVE - READY");
              printf("camera-ui: RESUMED preview only\n");
              frames++;
              continue;
            }
        }
      else if (touch_fd >= 0)
        {
          struct touch_sample_s sample;
          int events = 0;
          while (events++ < 64 && read(touch_fd, &sample, sizeof(sample)) == sizeof(sample))
            {
              int x = sample.point[0].x;
              int y = sample.point[0].y;
              printf("touch: id=%u flags=%02x x=%d y=%d\n",
                     sample.point[0].id, sample.point[0].flags, x, y);
              if (sample.npoints && x >= 0 && x < 800 && y >= 0 && y < 480)
                {
                  int dx, dy;
                  for (dy = -5; dy <= 5; dy++)
                    for (dx = -5; dx <= 5; dx++)
                      if (x+dx >= 0 && x+dx < 800 && y+dy >= 0 && y+dy < 480)
                        ((uint16_t *)(display+(y+dy)*plane.stride))[x+dx] = 0xf800;
                }
            }
        }
#endif

      if(profile){monotonic_ms(&stamp);stages[3]+=stamp-mark;mark=stamp;}
      /* S31 FBIO_UPDATE copies to a spare scanout buffer and waits for the
       * DMA switch. Only after it returns may the mapped drawing be reused.
       */

      if (checked_ioctl(fb_fd, FBIO_UPDATE, &area, "FBIO_UPDATE") < 0 ||
          monotonic_ms(&now) < 0)
        {
          goto out;
        }

      if(profile){monotonic_ms(&stamp);stages[4]+=stamp-mark;}
      frames++;
      if(profile && frames%30==0)printf("preview-profile: frames=%lu elapsed_ms=%llu dq=%llu record=%llu copy=%llu ui=%llu lcd=%llu (totals ms)\n",(unsigned long)frames,(unsigned long long)(now-start),(unsigned long long)stages[0],(unsigned long long)stages[1],(unsigned long long)stages[2],(unsigned long long)stages[3],(unsigned long long)stages[4]);
#ifdef CONFIG_INPUT_TOUCHSCREEN
      if (!ui_mode)
#endif
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

#ifdef CONFIG_INPUT_TOUCHSCREEN
  /* The LCD keeps scanning after this application exits. Replace the
   * last live frame so a normal return does not look like a frozen UI. */
  if (ui_mode && display != MAP_FAILED && fb_fd >= 0)
    {
      ui_rect(display, plane.stride, 0, 0, 800, 480, 0x1082);
      ui_text(display, plane.stride, 200, 200,
              result == EXIT_SUCCESS ? "CAMERA CLOSED" : "CAMERA ERROR");
      ui_text(display, plane.stride, 104, 248, return_home ? "RETURNING TO HOME" : "RESTART FROM SERIAL CONSOLE");
      if (checked_ioctl(fb_fd, FBIO_UPDATE, &area, "exit FBIO_UPDATE") < 0)
        result = EXIT_FAILURE;
    }
#endif

#ifndef CONFIG_BUILD_FLAT
  if (display != MAP_FAILED && munmap(display, display_size) < 0)
    {
      fprintf(stderr, "s31preview: framebuffer munmap failed errno=%d\n",
              errno);
      result = EXIT_FAILURE;
    }

#endif

  if (record_close(&recording) < 0)
    { perror("s31preview record close"); result = EXIT_FAILURE; }
  if (record_path) printf("record: path=%s frames=%" PRIu32 " format=YUV4MPEG2\n", record_path, recording.frames);

#ifdef CONFIG_INPUT_TOUCHSCREEN
  if (ui.fd >= 0) close(ui.fd);
  free(ui.pixels); free(ui.scratch);
  if (ui.mounted && umount("/mnt/sdcard") < 0) result = EXIT_FAILURE;
#endif
  if (touch_fd >= 0) close(touch_fd);

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
