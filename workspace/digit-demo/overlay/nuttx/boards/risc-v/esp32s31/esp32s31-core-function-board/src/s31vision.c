/* SPDX-License-Identifier: Apache-2.0 */
/* Independent kernel service. No user virtual addresses reach DMA/HPWORK.
 * Three capture slots have explicit ownership. The AI mailbox is single
 * outstanding/latest-at-admission: video never waits for inference. */
#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/sched.h>
#include <nuttx/kthread.h>
#include <nuttx/semaphore.h>
#include <nuttx/spinlock.h>
#include <nuttx/video/fb.h>
#include <nuttx/clock.h>
#include <nuttx/signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sched.h>
#include <syslog.h>
#include "esp32s31_camera_dvp.h"
#include "vision_infer.h"
#include "vision_font.h"

extern void *s31vision_dma_alloc(size_t, size_t);
extern uint16_t *s31vision_lcd_acquire(void);
extern int s31vision_lcd_present(void);
extern void s31vision_lcd_probe(void);
extern unsigned s31vision_lcd_displayed(void);
extern void s31vision_camera_stats(void);
extern void s31vision_sensor_stats(void);
extern void *s31vision_camera_buffer(unsigned index);
extern int s31vision_camera_raw_callback(void (*callback)(unsigned));
extern void s31vision_camera_release(unsigned index);
#define W 640
#define H 480
#define BYTES (W*H*2)
#define ROI 200
static uint16_t *frames[3];
static enum { FREE, CAPTURE, READY, DISPLAY } state[3];
static int ready_slot = -1;
static spinlock_t lock = SP_UNLOCKED;
static sem_t frame_sem = SEM_INITIALIZER(0);
static sem_t ai_sem = SEM_INITIALIZER(0);
static sem_t ai_ready = SEM_INITIALIZER(0);
static uint8_t *gray;
static bool ai_busy;
static int answer = -1, ai_error;
static unsigned captured, inferred, frame_id, result_id;
static uint64_t infer_us, result_time, submitted_time;
static unsigned confidence;
static int8_t result_input[784];

static uint64_t now_us(void)
{
  struct timespec t;
  clock_systime_timespec(&t);
  return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;
}
static int pin(int cpu)
{
  cpu_set_t set = (cpu_set_t)1 << cpu;
  return sched_setaffinity(0, sizeof(set), &set);
}
static void complete(unsigned index)
{
  irqstate_t flags = spin_lock_irqsave(&lock);
  bool notify = ready_slot < 0;
  int discard=ready_slot;
  ready_slot=index;state[index]=READY;captured++;
  spin_unlock_irqrestore(&lock,flags);
  if(discard>=0)s31vision_camera_release(discard);
  if(notify)nxsem_post(&frame_sem);
}
static void text(uint16_t *fb,int x,int y,const char *s,int scale)
{
  while (*s && x+8*scale<=800)
    {
      unsigned c=(unsigned char)*s++;
      if(c>=32 && c<127) for(int j=0;j<13;j++) for(int i=0;i<8;i++)
        if(ui_font[c-32][j]&(128>>i))
          for(int dy=0;dy<scale;dy++) for(int dx=0;dx<scale;dx++)
            fb[(y+j*scale+dy)*800+x+i*scale+dx]=0xffff;
      x+=8*scale;
    }
}
#include "vision_preprocess.h"
static int ai_main(int argc,char **argv)
{
  (void)argc;(void)argv;
  if(pin(1)<0){ai_error=1;nxsem_post(&ai_ready);return 1;}
  /* Full model dot parity, not just checking a single short SIMD operation. */
  unsigned errors=0, correct=0;float scores[10];
  uint64_t start=now_us();
  for(int i=0;i<100;i++)
    {
      const int8_t *v=digit_test_images+i*784;
      for(int n=0;n<64;n++)
        {
          int32_t scalar=0;
          for(int j=0;j<784;j++)scalar+=(int32_t)v[j]*digit_w1[n*784+j];
          if(scalar!=s31digit_dot(v,digit_w1+n*784,784))errors++;
        }
      correct+=digit_infer(v,scores)==digit_test_labels[i];
    }
  syslog(LOG_INFO,"VISION AI cpu=%d SIMD parity_errors=%u heldout=%u/100 elapsed_us=%llu\n",
         sched_getcpu(),errors,correct,(unsigned long long)(now_us()-start));
  ai_error=errors||correct!=100;nxsem_post(&ai_ready);
  if(ai_error)return 1;
  int8_t input[784] __attribute__((aligned(16)));
  int previous=-1;unsigned consecutive=0;
  for(;;)
    {
      if(nxsem_wait_uninterruptible(&ai_sem)<0)continue;
      start=now_us();int digit=-1,best_digit=-1;unsigned conf=0;
      float margin=0;
      memset(input,0,sizeof(input));
      int prep=vision_preprocess(gray,input);
      if(prep==0)
        {
          int b=digit_infer(input,scores),second=b==0?1:0;
          for(int i=0;i<10;i++)if(i!=b&&scores[i]>scores[second])second=i;
          float sum=0;
          for(int i=0;i<10;i++)sum+=expf(scores[i]-scores[b]);
          conf=(unsigned)(100.0f/sum);
          best_digit=b; margin=scores[b]-scores[second];
          if(margin>=0.35f && conf>=40)digit=b;
        }
      irqstate_t flags=spin_lock_irqsave(&lock);
      if(digit>=0 && digit==previous)consecutive++;else consecutive=1;
      previous=digit;
      /* Moderate candidates require four consecutive matching frames.
       * Never special-case a digit or accept rejected/blank crops. */
      unsigned required=(conf>=90 && margin>=3.0f)?1:
                        (conf>=70 && margin>=2.0f)?2:4;
      answer=(digit>=0 && consecutive>=required)?digit:-1;
      confidence=answer<0?0:conf;
      memcpy(result_input,input,sizeof(input));
      infer_us=now_us()-start;inferred++;
      result_time=submitted_time;result_id=frame_id;ai_busy=false;
      unsigned sample=inferred;
      spin_unlock_irqrestore(&lock,flags);
      if(sample%8==0)
        syslog(LOG_INFO,"VISION CLASSIFY sample=%u prep=%d best=%d confidence=%u margin_x100=%d accepted=%d\n",
          sample,prep,best_digit,conf,(int)(margin*100),digit);
    }
  return 0;
}
static int video_main(int argc,char **argv)
{
  (void)argc;(void)argv;
  if(pin(0)<0)return 1;
  struct fb_vtable_s *v=up_fbgetvplane(0,0);
  struct fb_planeinfo_s plane;
  if(!v||v->getplaneinfo(v,0,&plane)<0)return 1;
  uint16_t *fb;
  for(int i=0;i<3;i++)
    {frames[i]=s31vision_camera_buffer(i);if(!frames[i])return 1;state[i]=FREE;}
  gray=s31vision_dma_alloc(64,ROI*ROI+280*280);
  if(!gray)return 1;
  if(kthread_create("vision-ai",100,16384,ai_main,NULL)<0)return 1;
  nxsem_wait_uninterruptible(&ai_ready);
  if(ai_error)return 1;
  s31vision_lcd_probe();
  int ret=s31vision_camera_raw_callback(complete);
  if(ret>=0)ret=esp32s31_camera_dvp_start();
  if(ret<0){syslog(LOG_ERR,"VISION capture start=%d\n",ret);return 1;}
  syslog(LOG_INFO,"VISION START cpu=%d VGA=640x480 ROI=200x200 recording=OFF\n",sched_getcpu());
  uint64_t start=now_us(),copy_us=0,roi_us=0,lcd_us=0;unsigned shown=0;char buf[32];
  for(;;)
    {
      nxsem_wait_uninterruptible(&frame_sem);
      irqstate_t flags=spin_lock_irqsave(&lock);
      int index=ready_slot;ready_slot=-1;
      if(index<0){spin_unlock_irqrestore(&lock,flags);continue;}
      state[index]=DISPLAY;
      bool submit=!ai_busy;
      if(submit){ai_busy=true;frame_id=captured;submitted_time=now_us();}
      int digit=now_us()-result_time<1000000?answer:-1;
      unsigned cap=captured,inf=inferred,rid=result_id;
      uint64_t us=infer_us;unsigned conf=confidence;
      int8_t thumbnail[784];memcpy(thumbnail,result_input,sizeof(thumbnail));
      spin_unlock_irqrestore(&lock,flags);
      uint64_t stamp=now_us();
      fb=s31vision_lcd_acquire();
      if(!fb)return 1;
      for(int y=0;y<H;y++)
        {
          volatile uint32_t *dst=(volatile uint32_t *)(fb+y*800);
          /* Keep the vertical correction, but do not mirror the scene.
           * Swap bytes within each RGB565 pixel without reversing pixels.
           * Inference reads this same corrected display image below. */
          const uint32_t *src=(const uint32_t *)(frames[index]+(H-1-y)*W);
          for(int x=0;x<W/2;x++)
            {
              uint32_t pixels=src[x];
              dst[x]=((pixels & 0x00ff00ffu)<<8) |
                     ((pixels & 0xff00ff00u)>>8);
            }
        }
      copy_us+=now_us()-stamp;stamp=now_us();
      if(submit)
        {
          for(int y=0;y<ROI;y++)for(int x=0;x<ROI;x++)
            {
              unsigned p=fb[(y+140)*800+x+220];
              gray[y*ROI+x]=(((p>>11)*255/31)*77+(((p>>5)&63)*255/63)*150+((p&31)*255/31)*29)>>8;
            }
          nxsem_post(&ai_sem);
        }
      roi_us+=now_us()-stamp;
      s31vision_camera_release(index);
      for(int y=0;y<H;y++)memset(fb+y*800+W,0,(800-W)*2);
      for(int x=220;x<420;x++){fb[140*800+x]=0x07e0;fb[339*800+x]=0x07e0;}
      for(int y=140;y<340;y++){fb[y*800+220]=0x07e0;fb[y*800+419]=0x07e0;}
      text(fb,650,20,"VISION AI",1);text(fb,650,45,"CPU1 SIMD",1);
      buf[0]=digit<0?'-':'0'+digit;buf[1]=0;text(fb,670,90,buf,8);
      uint64_t elapsed=now_us()-start;
      snprintf(buf,sizeof(buf),"FPS %u.%u",elapsed?(unsigned)(s31vision_lcd_displayed()*10000000ull/elapsed)/10:0,
               elapsed?(unsigned)(s31vision_lcd_displayed()*10000000ull/elapsed)%10:0);text(fb,650,240,buf,1);
      snprintf(buf,sizeof(buf),"AI %llu us",(unsigned long long)us);text(fb,650,270,buf,1);
      snprintf(buf,sizeof(buf),"CONF %u%%",digit<0?0:conf);text(fb,650,290,buf,1);
      for(int y=0;y<28;y++)for(int x=0;x<28;x++)
        {
          unsigned g=(unsigned)thumbnail[y*28+x]*255/127;
          uint16_t color=((g>>3)<<11)|((g>>2)<<5)|(g>>3);
          for(int dy=0;dy<4;dy++)for(int dx=0;dx<4;dx++)
            fb[(312+y*4+dy)*800+660+x*4+dx]=color;
        }
      text(fb,650,440,"SHOW ONE DIGIT",1);text(fb,650,458,"IN GREEN BOX",1);
      stamp=now_us();ret=s31vision_lcd_present();lcd_us+=now_us()-stamp;
      if(ret<0){syslog(LOG_ERR,"VISION LCD update=%d\n",ret);return 1;}
      shown++;
      if(shown%60==0)
        {
        syslog(LOG_INFO,"VISION METRIC cpu=%d captured=%u queued=%u displayed=%u elapsed_us=%llu infer=%u result_frame=%u digit=%d\n",
          sched_getcpu(),cap,shown,s31vision_lcd_displayed(),(unsigned long long)(now_us()-start),inf,rid,digit);
        s31vision_camera_stats();
        if(shown==60)s31vision_sensor_stats();
        syslog(LOG_INFO,"VISION STAGES copy_us=%llu roi_us=%llu lcd_us=%llu\n",
          (unsigned long long)copy_us,(unsigned long long)roi_us,(unsigned long long)lcd_us);
        }
    }
  return 0;
}
int s31vision_start(void)
{
  return kthread_create("vision-video",110,8192,video_main,NULL)<0?-EIO:0;
}
