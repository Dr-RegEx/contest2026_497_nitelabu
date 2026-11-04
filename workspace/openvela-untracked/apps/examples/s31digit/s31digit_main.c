/* SPDX-License-Identifier: Apache-2.0 */
#include <nuttx/config.h>
#include <nuttx/video/fb.h>
#include <nuttx/input/touchscreen.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include "digit_infer.h"
#include "../s31preview/s31ui_font.h"
#ifdef CONFIG_EXAMPLES_S31DIGIT_SIMD
#define BACKEND "CPU1 PIE SIMD"
#else
#define BACKEND "CPU SCALAR INT8"
#endif
static uint64_t micros(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000; }
static void rect(uint8_t *fb,int stride,int x,int y,int w,int h,uint16_t c)
{ for(int j=y;j<y+h&&j<480;j++)for(int i=x;i<x+w&&i<800;i++)if(i>=0&&j>=0)((uint16_t *)(fb+j*stride))[i]=c; }
static void text(uint8_t *fb,int stride,int x,int y,const char *s,int scale)
{ for(;*s;s++,x+=8*scale){unsigned c=(unsigned char)*s;if(c<32||c>126)c='?';for(int j=0;j<13;j++)for(int i=0;i<8;i++)if(ui_font[c-32][j]&(128>>i))rect(fb,stride,x+i*scale,y+j*scale,scale,scale,0xffff);} }
static void brush(uint8_t *canvas,int x,int y)
{ for(int j=-7;j<=7;j++)for(int i=-7;i<=7;i++)if(i*i+j*j<=49&&x+i>=0&&x+i<280&&y+j>=0&&y+j<280)canvas[(y+j)*280+x+i]=255; }
static void line(uint8_t *c,int x,int y,int a,int b)
{ int n=abs(a-x)>abs(b-y)?abs(a-x):abs(b-y);if(!n){brush(c,x,y);return;}for(int k=0;k<=n;k++)brush(c,x+(a-x)*k/n,y+(b-y)*k/n); }
static int selftest(void)
{
 int correct=0;uint64_t start=micros();float scores[10];
 for(int i=0;i<100;i++) {int pred=digit_infer(digit_test_images+i*784,scores);correct+=pred==digit_test_labels[i];}
 printf("s31digit: backend=%s MNIST first100 correct=%d/100 total_us=%llu\n",BACKEND,correct,(unsigned long long)(micros()-start));
 return correct>=90?0:1;
}
static int run(int argc,char **argv)
{
 if(argc>1&&!strcmp(argv[1],"--selftest"))return selftest();
 bool diagnostic=argc>1&&!strcmp(argv[1],"--diagnostic");
 bool demo=argc>1&&!strcmp(argv[1],"--demo");
 int fd=open("/dev/fb0",O_RDWR),td=-1,ret=1;uint8_t *fb=MAP_FAILED,*c=NULL;size_t mapped=0;
 struct fb_videoinfo_s vi;struct fb_planeinfo_s p={0};struct fb_area_s area={0,0,800,480};
 int8_t input[784] __attribute__((aligned(16)));float scores[10];
 if(fd<0||ioctl(fd,FBIOGET_VIDEOINFO,(unsigned long)&vi)<0||ioctl(fd,FBIOGET_PLANEINFO,(unsigned long)&p)<0)goto done;
 if(vi.xres!=800||vi.yres!=480||vi.fmt!=FB_FMT_RGB16_565||p.bpp!=16||p.stride<1600||p.fblen<p.stride*480)goto done;
#ifdef CONFIG_BUILD_FLAT
 fb=p.fbmem;
#else
 mapped=p.stride*480;fb=mmap(NULL,mapped,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
#endif
 if(fb==MAP_FAILED)goto done;
 td=open("/dev/input0",O_RDONLY|O_NONBLOCK);c=calloc(280,280);if(td<0||!c)goto done;
 bool dirty=true,down=false,quit=false;int lastx=0,lasty=0,id=-1,answer=-1,shown=0;
 uint64_t us=0,lastdraw=0,nextdemo=0;char label[80];
 printf("s31digit: READY backend=%s; draw digit then RECOGNIZE; no files saved\n",BACKEND);
 while(!quit) {
  struct touch_sample_s s;int count=0;bool recognize=false;
  while(count++<128&&read(td,&s,sizeof(s))==sizeof(s)) {
   if(!s.npoints)continue;
   int x=s.point[0].x,y=s.point[0].y,flags=s.point[0].flags;
   if(flags&TOUCH_UP){down=false;id=-1;continue;}
   if((flags&TOUCH_DOWN)&&y>=420) {
    down=false;id=-1;
    if(x<260)recognize=true;
    else if(x<530){memset(c,0,280*280);answer=-1;dirty=true;}
    else quit=true;
    continue;
   }
   if(x>=40&&x<400&&y>=50&&y<410) {
    int nx=(x-40)*280/360,ny=(y-50)*280/360;
    if(down&&id==s.point[0].id)line(c,lastx,lasty,nx,ny);else brush(c,nx,ny);
    down=true;id=s.point[0].id;lastx=nx;lasty=ny;answer=-1;dirty=true;
   }else{down=false;id=-1;}
  }
  if(demo&&micros()>=nextdemo) {
   if(shown==10){quit=true;continue;}
   memcpy(input,digit_test_images+shown*784,784);
   for(int y=0;y<280;y++)for(int x=0;x<280;x++)c[y*280+x]=input[(y/10)*28+x/10]*2;
   uint64_t t=micros();answer=digit_infer(input,scores);us=micros()-t;
   printf("s31digit demo: held-out sample=%d label=%d predicted=%d tick_measured_us=%llu\n",shown,digit_test_labels[shown],answer,(unsigned long long)us);
   shown++;dirty=true;nextdemo=micros()+2000000;
  }
  if(recognize) {
   if(digit_normalize(c,input)==0){uint64_t t=micros();answer=digit_infer(input,scores);us=micros()-t;printf("s31digit: handwriting predicted=%d tick_measured_us=%llu backend=%s\n",answer,(unsigned long long)us,BACKEND);}
   else answer=-1;
   if(diagnostic&&answer>=0) {
    printf("s31digit input28 begin predicted=%d\n",answer);
    for(int y=0;y<28;y++) {
     char row[57];
     for(int x=0;x<28;x++)snprintf(row+x*2,3,"%02x",(unsigned char)input[y*28+x]);
     printf("input28 %s\n",row);
    }
    printf("s31digit input28 end\n");
   }
   dirty=true;
  }
  if(dirty&&micros()-lastdraw>=16000) {
   rect(fb,p.stride,0,0,800,480,0x1082);
   text(fb,p.stride,40,10,"WRITE ONE DIGIT 0-9",2);
   for(int y=0;y<360;y++)for(int x=0;x<360;x++){unsigned v=c[(y*280/360)*280+x*280/360];((uint16_t *)(fb+(y+50)*p.stride))[x+40]=((v>>3)<<11)|((v>>2)<<5)|(v>>3);}
   text(fb,p.stride,440,60,"RESULT",3);
   if(answer>=0){snprintf(label,sizeof(label),"%d",answer);text(fb,p.stride,500,110,label,10);if(us<CONFIG_USEC_PER_TICK)snprintf(label,sizeof(label),"<%d ms",CONFIG_USEC_PER_TICK/1000);else snprintf(label,sizeof(label),"~%llu ms",(unsigned long long)(us/1000));text(fb,p.stride,440,265,label,2);}
   else text(fb,p.stride,450,145,"?",8);
   if(answer>=0) {
    text(fb,p.stride,650,45,"INPUT",2);
    for(int y=0;y<28;y++)for(int x=0;x<28;x++) {
     unsigned v=(unsigned char)input[y*28+x]*2;
     rect(fb,p.stride,650+x*4,80+y*4,4,4,((v>>3)<<11)|((v>>2)<<5)|(v>>3));
    }
   }
   text(fb,p.stride,435,320,BACKEND,2);
   text(fb,p.stride,435,354,demo?"MNIST SAMPLES":"OFFLINE / ON DEVICE",2);
   rect(fb,p.stride,0,420,256,60,0x0320);text(fb,p.stride,15,435,"RECOGNIZE",2);
   rect(fb,p.stride,265,420,256,60,0x0210);text(fb,p.stride,300,435,"CLEAR",2);
   rect(fb,p.stride,530,420,270,60,0x6000);text(fb,p.stride,600,435,"EXIT",2);
   if(ioctl(fd,FBIO_UPDATE,(unsigned long)&area)<0)goto done;
   dirty=false;lastdraw=micros();
  }
  usleep(5000);
 }
 rect(fb,p.stride,0,0,800,480,0x1082);text(fb,p.stride,160,200,"DIGIT DEMO CLOSED",3);ioctl(fd,FBIO_UPDATE,(unsigned long)&area);ret=0;
done:
 if(ret)perror("s31digit");
 free(c);if(td>=0)close(td);if(mapped&&fb!=MAP_FAILED)munmap(fb,mapped);if(fd>=0)close(fd);return ret;
}
#ifdef CONFIG_EXAMPLES_S31PREVIEW
extern int s31preview_main(int argc, char **argv);
/* Only the menu or one application owns the display and touch at a time. */
static int choose_app(void)
{
 int fd=open("/dev/fb0",O_RDWR),td=-1,choice=-1;
 struct fb_videoinfo_s v;struct fb_planeinfo_s p={0};
 struct fb_area_s area={0,0,800,480};uint8_t *fb=MAP_FAILED;size_t mapped=0;
 if(fd<0||ioctl(fd,FBIOGET_VIDEOINFO,(unsigned long)&v)<0||
    ioctl(fd,FBIOGET_PLANEINFO,(unsigned long)&p)<0)goto done;
 if(v.xres!=800||v.yres!=480||v.fmt!=FB_FMT_RGB16_565||p.bpp!=16||p.stride<1600||p.fblen<p.stride*480)goto done;
#ifdef CONFIG_BUILD_FLAT
 fb=p.fbmem;
#else
 mapped=p.stride*480;fb=mmap(NULL,mapped,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
#endif
 if(fb==MAP_FAILED)goto done;
 td=open("/dev/input0",O_RDONLY|O_NONBLOCK);if(td<0)goto done;
 rect(fb,p.stride,0,0,800,480,0x1082);
 text(fb,p.stride,160,45,"OPENVELA DEMO",4);
 rect(fb,p.stride,40,160,340,170,0x0320);
 rect(fb,p.stride,420,160,340,170,0x0258);
 text(fb,p.stride,130,222,"CAMERA",3);
 text(fb,p.stride,478,208,"HANDWRITE",3);
 text(fb,p.stride,523,255,"DIGITS",2);
 text(fb,p.stride,128,380,"EXIT RETURNS TO THIS MENU",2);
 if(ioctl(fd,FBIO_UPDATE,(unsigned long)&area)<0)goto done;
 printf("s31home: READY camera / handwritten digits\n");
 while(choice<0) {
  struct touch_sample_s s;
  ssize_t n=read(td,&s,sizeof(s));
  if(n<0&&errno!=EAGAIN&&errno!=EINTR)goto done;
  if(n==sizeof(s)&&s.npoints&&(s.point[0].flags&TOUCH_DOWN)) {
   int x=s.point[0].x,y=s.point[0].y;
   if(y>=160&&y<330){if(x>=40&&x<380)choice=0;else if(x>=420&&x<760)choice=1;}
  }
  if(choice<0)usleep(5000);
 }
done:
 if(td>=0)close(td);
 if(mapped&&fb!=MAP_FAILED)munmap(fb,mapped);
 if(fd>=0)close(fd);
 return choice;
}
static int home(bool test)
{
 char *camera[]={"s31preview","--ui","--return-home",NULL};
 char *shortcam[]={"s31preview","--ui","--return-home","--frames","2",NULL};
 char *digit[]={"s31digit",NULL};
 for(int round=0;;round++) {
  if(test) {
   if(round==3){printf("s31home: lifecycle 3 rounds PASS\n");return 0;}
   if(selftest()!=0||s31preview_main(5,shortcam)!=0)return 1;
   continue;
  }
  int selected=choose_app();if(selected<0)return 1;
  printf("s31home: entering %s\n",selected==0?"camera":"digits");
  int status=selected==0?s31preview_main(3,camera):run(1,digit);
  printf("s31home: returned %s status=%d\n",selected==0?"camera":"digits",status);
 }
}
#endif
static int dispatch(int argc,char **argv)
{
#ifdef CONFIG_EXAMPLES_S31PREVIEW
 if(argc==1||!strcmp(argv[1],"--home"))return home(false);
 if(!strcmp(argv[1],"--home-test"))return home(true);
#endif
 return run(argc,argv);
}
#ifdef CONFIG_EXAMPLES_S31DIGIT_SIMD
struct args {int argc;char **argv;int result;};
static void *worker(void *p){struct args *a=p;a->result=dispatch(a->argc,a->argv);return NULL;}
#endif
int main(int argc,char **argv)
{
#ifdef CONFIG_EXAMPLES_S31DIGIT_SIMD
 pthread_t t;pthread_attr_t at;cpu_set_t set;struct args a={argc,argv,1};
 CPU_ZERO(&set);CPU_SET(1,&set);pthread_attr_init(&at);
 int r=pthread_attr_setaffinity_np(&at,sizeof(set),&set);
 if(!r)r=pthread_attr_setstacksize(&at,16384);
 if(!r)r=pthread_create(&t,&at,worker,&a);
 pthread_attr_destroy(&at);if(r)return 1;pthread_join(t,NULL);return a.result;
#else
 return dispatch(argc,argv);
#endif
}
