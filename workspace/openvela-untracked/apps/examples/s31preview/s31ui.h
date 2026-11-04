/* SPDX-License-Identifier: Apache-2.0 */
/* Touch-operated capture and SD gallery. No capture is saved on startup. */
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include "s31ui_font.h"
struct s31ui_s {
  int mode; /* 0 preview, 1 list, 2 viewer */
  int confirm; /* 1 photo, 2 record, 3 delete selected file */
  int page, count, fd;
  char names[128][32];
  char status[64];
  char path[64];
  uint8_t *pixels, *scratch;
  bool movie, ended, mounted, quit;
  void (*photo_progress)(void *arg, unsigned percent);
  void *photo_progress_arg;
  uint64_t due;
  unsigned interval;
};
static int ui_read(int fd, void *p, size_t n)
{
  uint8_t *b=p;
  while(n) { ssize_t r=read(fd,b,n); if(r<0 && errno==EINTR)continue;
    if(r<=0)return -1;
    b+=r;n-=r; }
  return 0;
}
static void ui_rect(uint8_t *fb,size_t stride,int x,int y,int w,int h,uint16_t c)
{
  for(int j=y;j<y+h && j<480;j++)for(int i=x;i<x+w && i<800;i++)
    if(j>=0 && i>=0)((uint16_t *)(fb+j*stride))[i]=c;
}
static void ui_text(uint8_t *fb,size_t stride,int x,int y,const char *s)
{
  for(;*s && x<784;s++,x+=16) {
    unsigned c=(unsigned char)*s;if(c<32 || c>126)c='?';
    for(int j=0;j<13;j++)for(int i=0;i<8;i++)
      if(ui_font[c-32][j]&(128>>i))ui_rect(fb,stride,x+2*i,y+2*j,2,2,0xffff);
  }
}
static void ui_button(uint8_t *fb,size_t stride,int n,const char *s,uint16_t c)
{ ui_rect(fb,stride,n*160,420,156,60,c);ui_text(fb,stride,n*160+8,436,s); }
static void ui_error(struct s31ui_s *u,const char *action)
{ snprintf(u->status,sizeof(u->status),"%s ERROR %d",action,errno);printf("camera-ui: %s\n",u->status); }
static bool ui_sd(void)
{
  struct statfs fs;
  return statfs("/mnt/sdcard",&fs)==0 &&
    (fs.f_type==MSDOS_SUPER_MAGIC || fs.f_type==FATFS_SUPER_MAGIC);
}
static int ui_name(struct s31ui_s *u,const char *ext)
{
  struct stat st;
  if(!ui_sd()){errno=ENODEV;return -1;}
  for(int n=1;n<=99999;n++) {
    snprintf(u->path,sizeof(u->path),"/mnt/sdcard/C%05d.%s",n,ext);
    if(stat(u->path,&st)<0){if(errno==ENOENT)return 0;return -1;}
  }
  errno=ENOSPC;return -1;
}
static void ui_le32(uint8_t *p,uint32_t v)
{ for(int i=0;i<4;i++)p[i]=(v>>(8*i))&255; }
static uint32_t ui_u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static int ui_photo(struct s31ui_s *u,const uint8_t *rgb,size_t stride)
{
  uint8_t h[54]={0};int ret=0;size_t used=54;
  if(u->photo_progress)u->photo_progress(u->photo_progress_arg,0);
  if(ui_name(u,"BMP")<0)return -1;
  int fd=open(u->path,O_WRONLY|O_CREAT|O_EXCL,0666);if(fd<0)return -1;
  h[0]='B';h[1]='M';ui_le32(h+2,54+640*480*3);ui_le32(h+10,54);
  ui_le32(h+14,40);ui_le32(h+18,640);ui_le32(h+22,480);h[26]=1;h[28]=24;
  ui_le32(h+34,640*480*3);
  /* Pack the header and rows into sector-aligned 64KiB transfers. Writing
   * each 1920-byte row separately after a 54-byte header repeatedly splits
   * sectors and prevents efficient multi-block SD writes. */
  memcpy(u->scratch,h,sizeof(h));
  for(int y=479;y>=0 && ret==0;y--) {
    for(int x=0;x<640 && ret==0;x++) {
      int r,g,b;record_rgb(rgb+y*stride+2*x,&r,&g,&b);
      uint8_t pixel[3]={(uint8_t)b,(uint8_t)g,(uint8_t)r};
      for(int k=0;k<3;k++) {
        u->scratch[used++]=pixel[k];
        if(used==65536) {
          if(record_write(fd,u->scratch,used)<0){ret=-1;break;}
          used=0;
        }
      }
    }
    if(ret==0 && (y%64==0) && u->photo_progress)
      u->photo_progress(u->photo_progress_arg,(unsigned)(480-y)*95/480);
  }
  if(ret==0 && used && record_write(fd,u->scratch,used)<0)ret=-1;
  if(ret==0 && u->photo_progress)u->photo_progress(u->photo_progress_arg,99);
  if(fsync(fd)<0)ret=-1;
  if(close(fd)<0)ret=-1;
  if(ret==0 && u->photo_progress)u->photo_progress(u->photo_progress_arg,100);
  return ret;
}
static int ui_compare(const void *a,const void *b){return strcmp(a,b);}
static void ui_list(struct s31ui_s *u)
{
  u->count=0;u->page=0;u->mode=1;
  DIR *d=opendir("/mnt/sdcard");if(!d){ui_error(u,"SD");return;}
  struct dirent *e;
  while((e=readdir(d))!=NULL && u->count<128) {
    const char *ext=strrchr(e->d_name,'.');size_t len=strlen(e->d_name);
    if(!ext || len>=32 || (strcasecmp(ext,".BMP") && strcasecmp(ext,".Y4M")))continue;
    struct stat st;char path[64];snprintf(path,sizeof(path),"/mnt/sdcard/%.31s",e->d_name);
    if(stat(path,&st)<0 || !S_ISREG(st.st_mode))continue;
    strcpy(u->names[u->count++],e->d_name);
  }
  closedir(d);qsort(u->names,u->count,sizeof(u->names[0]),ui_compare);
  snprintf(u->status,sizeof(u->status),"SD FILES: %d (BMP / Y4M)",u->count);
}
static uint8_t ui_clamp(int v){return v<0?0:v>255?255:v;}
static int ui_movie_frame(struct s31ui_s *u)
{
  char mark[6];if(ui_read(u->fd,mark,6)<0 || memcmp(mark,"FRAME\n",6) ||
    ui_read(u->fd,u->scratch,460800)<0)return -1;
  for(int y=0;y<480;y++)for(int x=0;x<640;x++) {
    int c=u->scratch[y*640+x]-16;
    int d=u->scratch[307200+(y/2)*320+x/2]-128;
    int e=u->scratch[384000+(y/2)*320+x/2]-128;
    unsigned r=ui_clamp((298*c+409*e+128)>>8),g=ui_clamp((298*c-100*d-208*e+128)>>8),b=ui_clamp((298*c+516*d+128)>>8);
    ((uint16_t *)u->pixels)[y*640+x]=((r>>3)<<11)|((g>>2)<<5)|(b>>3);
  }
  return 0;
}
static void ui_view(struct s31ui_s *u,int index,uint64_t now)
{
  snprintf(u->path,sizeof(u->path),"/mnt/sdcard/%s",u->names[index]);
  u->fd=open(u->path,O_RDONLY);if(u->fd<0){ui_error(u,"OPEN");return;}
  u->movie=strcasecmp(strrchr(u->path,'.'),".Y4M")==0;u->ended=false;
  if(u->movie){
    char h[160];size_t n=0;unsigned w,ht,num,den;
    while(n<sizeof(h)-1 && ui_read(u->fd,h+n,1)==0){if(h[n++]=='\n')break;}
    h[n]=0;
    if(sscanf(h,"YUV4MPEG2 W%u H%u F%u:%u",&w,&ht,&num,&den)!=4 ||
       w!=640 || ht!=480 || !num || !den || !strstr(h,"C420jpeg") ||
       ui_movie_frame(u)<0)goto invalid;
    uint64_t interval=(uint64_t)den*1000/num;
    if(!interval || interval>60000)goto invalid;
    u->interval=interval;u->due=now+u->interval;
  }else{
    uint8_t h[54];if(ui_read(u->fd,h,54)<0 || h[0]!='B' || h[1]!='M' ||
      ui_u32(h+14)!=40 || ui_u32(h+18)!=640 || ui_u32(h+22)!=480 ||
      h[26]!=1 || h[27] || h[28]!=24 || h[29] || ui_u32(h+30))goto invalid;
    if(lseek(u->fd,ui_u32(h+10),SEEK_SET)<0)goto invalid;
    for(int y=479;y>=0;y--){if(ui_read(u->fd,u->scratch,1920)<0)goto invalid;
      for(int x=0;x<640;x++){uint8_t *p=u->scratch+x*3;
        ((uint16_t *)u->pixels)[y*640+x]=((p[2]>>3)<<11)|((p[1]>>2)<<5)|(p[0]>>3);}}
  }
  u->mode=2;snprintf(u->status,sizeof(u->status),"%s",u->names[index]);return;
invalid:
  close(u->fd);u->fd=-1;errno=EINVAL;ui_error(u,"FORMAT");
}
static void ui_step(struct s31ui_s *u,struct s31record_s *r,uint8_t *fb,size_t stride,
                    const uint8_t *rgb,size_t rgbstride,int touchfd,uint64_t now)
{
  struct touch_sample_s s;int action=-1;
  for(int n=0;n<64 && read(touchfd,&s,sizeof(s))==sizeof(s);n++)
    if(s.npoints && (s.point[0].flags&TOUCH_DOWN)) {
      int x=s.point[0].x,y=s.point[0].y;
      if(x>=0 && x<800 && y>=420 && y<480)action=x/160;
      else if(!u->confirm && u->mode==1 && y>=60 && y<410){int i=u->page*6+(y-60)/56;if(i<u->count)ui_view(u,i,now);}
    }
  if(action>=0) {
    if(u->confirm) {
      int choice=u->confirm;u->confirm=0;
      if(action==0){
        if(choice==3){
          if(u->mode==2 && r->fd<0){
            if(u->fd>=0){close(u->fd);u->fd=-1;}
            if(unlink(u->path)<0){int error=errno;ui_list(u);errno=error;ui_error(u,"DELETE");}
            else {printf("camera-ui: deleted %s\n",u->path);ui_list(u);snprintf(u->status,sizeof(u->status),"FILE DELETED");}
          }
        }
        else if(choice==1){if(ui_photo(u,rgb,rgbstride)<0)ui_error(u,"PHOTO");else snprintf(u->status,sizeof(u->status),"SAVED %s",strrchr(u->path, '/')+1);}
        else {if(ui_name(u,"Y4M")<0)ui_error(u,"SD");else {memset(r,0,sizeof(*r));r->fd=-1;
          if(record_open(r,u->path)<0){ui_error(u,"RECORD");record_close(r);}else snprintf(u->status,sizeof(u->status),"RECORDING %s",strrchr(u->path, '/')+1);}}
      }
    }else if(u->mode==0){
      if(action==0 && r->fd<0)u->confirm=1;
      if(action==1){if(r->fd>=0){if(record_close(r)<0)ui_error(u,"SAVE");else snprintf(u->status,sizeof(u->status),"SAVED %s",strrchr(u->path, '/')+1);}else u->confirm=2;}
      if(action==2 && r->fd<0)ui_list(u);
      if(action==4 && r->fd<0)u->quit=true;
    }else if(u->mode==1){
      if(action==0)u->mode=0;
      if(action==1 && u->page>0)u->page--;
      if(action==2 && (u->page+1)*6<u->count)u->page++;
      if(action==3)ui_list(u);
    }else if(action==0){if(u->fd>=0)close(u->fd);u->fd=-1;ui_list(u);}
    else if(u->mode==2 && action==4 && r->fd<0)u->confirm=3;
  }
  if(u->mode==2){
    if(!u->confirm && u->movie && !u->ended && now>=u->due){
      if(ui_movie_frame(u)<0){u->ended=true;snprintf(u->status,sizeof(u->status),"PLAYBACK ENDED");}
      u->due=now+u->interval;
    }
    copy_frame_pixels(fb,stride,u->pixels,1280);
  }else if(u->mode==1){
    ui_rect(fb,stride,0,0,800,480,0x1082);
    for(int n=0;n<6;n++){int i=u->page*6+n;if(i>=u->count)break;
      ui_rect(fb,stride,8,60+n*56,784,50,0x2104);ui_text(fb,stride,20,72+n*56,u->names[i]);}
  }
  ui_rect(fb,stride,0,0,800,42,r->fd>=0?0x9800:0x1082);
  ui_text(fb,stride,12,8,u->status);
  ui_rect(fb,stride,0,420,800,60,0x1082);
  if(u->confirm){
    ui_rect(fb,stride,16,318,768,96,0x1082);
    if(u->confirm==3)ui_text(fb,stride,30,326,strrchr(u->path,'/')+1);
    ui_text(fb,stride,30,360,u->confirm==3?"DELETE THIS FILE?":u->confirm==1?"SAVE PHOTO?":"START RECORDING?");ui_button(fb,stride,0,"CONFIRM",0x0320);ui_button(fb,stride,1,"CANCEL",0x7800);}
  else if(u->mode==0){ui_button(fb,stride,0,"PHOTO",0x0320);ui_button(fb,stride,1,r->fd>=0?"STOP":"RECORD",0x9800);ui_button(fb,stride,2,"FILES",0x0210);ui_button(fb,stride,4,"EXIT",0x4208);}
  else {ui_button(fb,stride,0,"BACK",0x0210);if(u->mode==2)ui_button(fb,stride,4,"DELETE",0x9800);if(u->mode==1){ui_button(fb,stride,1,"PREV",0x4208);ui_button(fb,stride,2,"NEXT",0x4208);ui_button(fb,stride,3,"REFRESH",0x4208);}}
}
