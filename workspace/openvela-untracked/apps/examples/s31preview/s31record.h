/* SPDX-License-Identifier: Apache-2.0 */
/* Uncompressed YUV4MPEG2 recording: portable, no encoder dependency.
 * RGB565 input is normalized LE; output is limited-range BT.601 YUV420.
 * O_EXCL protects existing files. Header rate is finalized from capture time.
 */
#include <sys/statfs.h>
#include <sys/time.h>

struct s31record_s {
  int fd;
  uint8_t *yuv;
  uint32_t frames;
  int64_t first_us;
  int64_t last_us;
  off_t rate_offset;
};

static int record_write(int fd, const void *buffer, size_t count)
{
  const uint8_t *p = buffer;
  while (count) {
    ssize_t n = write(fd, p, count);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { if (!n) errno = EIO; return -1; }
    p += n; count -= n;
  }
  return 0;
}

static int record_open(struct s31record_s *r, const char *path)
{
  struct statfs fs;
  const char header[] = "YUV4MPEG2 W640 H480 F0000000040:0000000009 Ip A1:1 C420jpeg\n";
  if (statfs("/mnt/sdcard", &fs) < 0 ||
      (fs.f_type != MSDOS_SUPER_MAGIC && fs.f_type != FATFS_SUPER_MAGIC) ||
      strncmp(path, "/mnt/sdcard/", sizeof("/mnt/sdcard/") - 1) != 0 || strstr(path, "/../")) {
    errno = EINVAL; return -1;
  }
  r->yuv = malloc(6 + 640 * 480 * 3 / 2);
  if (!r->yuv) return -1;
  r->fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
  if (r->fd < 0) { free(r->yuv); r->yuv=NULL; return -1; }
  r->rate_offset = (off_t)(strchr(header, 'F') - header + 1);
  return record_write(r->fd, header, sizeof(header)-1);
}

static void record_rgb(const uint8_t *p, int *red, int *green, int *blue)
{
  static const uint8_t expand5[32]={0,8,16,25,33,41,49,58,66,74,82,90,99,107,115,123,132,140,148,156,165,173,181,189,197,206,214,222,230,239,247,255};
  static const uint8_t expand6[64]={0,4,8,12,16,20,24,28,32,36,40,45,49,53,57,61,65,69,73,77,81,85,89,93,97,101,105,109,113,117,121,125,130,134,138,142,146,150,154,158,162,166,170,174,178,182,186,190,194,198,202,206,210,215,219,223,227,231,235,239,243,247,251,255};
  unsigned v = p[0] | p[1] << 8;
  *red = expand5[v >> 11];
  *green = expand6[(v >> 5) & 63];
  *blue = expand5[v & 31];
}

static int record_frame(struct s31record_s *r, const uint8_t *rgb,
                        size_t stride, const struct timeval *tv)
{
  const unsigned width=640, height=480;
  uint8_t *y=r->yuv+6, *u=y+width*height, *v=u+width*height/4;
  unsigned row,col,dy,dx;
  for (row=0;row<height;row+=2) {
    for (col=0;col<width;col+=2) {
      int sr=0,sg=0,sb=0;
      for (dy=0;dy<2;dy++) for (dx=0;dx<2;dx++) {
        int red,green,blue;
        record_rgb(rgb+(row+dy)*stride+2*(col+dx), &red,&green,&blue);
        y[(row+dy)*width+col+dx]=((66*red+129*green+25*blue+128)>>8)+16;
        sr+=red;sg+=green;sb+=blue;
      }
      sr=(sr+2)/4;sg=(sg+2)/4;sb=(sb+2)/4;
      u[(row/2)*(width/2)+col/2]=((-38*sr-74*sg+112*sb+128)>>8)+128;
      v[(row/2)*(width/2)+col/2]=((112*sr-94*sg-18*sb+128)>>8)+128;
    }
  }
  memcpy(r->yuv,"FRAME\n",6);
  if (record_write(r->fd,r->yuv,6+width*height*3/2)<0) return -1;
  r->last_us=(int64_t)tv->tv_sec*1000000+tv->tv_usec;
  if (!r->frames) r->first_us=r->last_us;
  r->frames++;
  return 0;
}

static int record_close(struct s31record_s *r)
{
  int ret=0;
  if (r->fd >= 0) {
    if (r->frames > 1 && r->last_us > r->first_us) {
      char rate[22];
      uint64_t num=(uint64_t)(r->frames-1)*1000;
      uint64_t den=(r->last_us-r->first_us)/1000;
      if (!den) den=1;
      while (num>9999999999ULL || den>9999999999ULL) {num=(num+1)/2;den=(den+1)/2;}
      snprintf(rate,sizeof(rate),"%010llu:%010llu",(unsigned long long)num,(unsigned long long)den);
      if (lseek(r->fd,r->rate_offset,SEEK_SET)<0 || record_write(r->fd,rate,21)<0) ret=-1;
    }
    if (fsync(r->fd)<0) ret=-1;
    if (close(r->fd)<0) ret=-1;
    r->fd=-1;
  }
  free(r->yuv);r->yuv=NULL;
  return ret;
}
