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
#include <nuttx/mutex.h>
#include <nuttx/kmalloc.h>
#include <nuttx/fs/fs.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <syslog.h>
#include "esp32s31_camera_dvp.h"
#include "vision_infer.h"
#include "vision_font.h"

extern void *s31vision_dma_alloc(size_t, size_t);
extern void s31vision_dma_free(void *);
extern uint16_t *s31vision_lcd_acquire(void);
extern int s31vision_lcd_present(void);
extern int s31vision_lcd_lease(bool acquire);
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
static int ready_slot = -1;
static spinlock_t lock = SP_UNLOCKED;
static sem_t frame_sem = SEM_INITIALIZER(0);
static sem_t ai_sem = SEM_INITIALIZER(0);
static sem_t ai_ready = SEM_INITIALIZER(0);
static sem_t ai_done = SEM_INITIALIZER(0);
static sem_t video_ready = SEM_INITIALIZER(0);
static sem_t video_done = SEM_INITIALIZER(0);
static mutex_t control = NXMUTEX_INITIALIZER;
static enum { STOPPED, STARTING, RUNNING, STOPPING, FAULT } service_state;
static bool stop_requested = true;
static bool video_active;
static bool camera_owned;
static bool callback_attached;
static bool display_owned;
static int last_error;
static unsigned displayed;
static bool preview_mode;
static mutex_t preview_lock = NXMUTEX_INITIALIZER;
static uint16_t *preview_pixels;
static unsigned preview_sequence;
static mutex_t owner_lock = NXMUTEX_INITIALIZER;
static void *owner;
static uint8_t *gray;
static bool ai_busy;
static int answer = -1, ai_error;
static unsigned captured, inferred, frame_id, result_id;
static uint64_t infer_us, result_time, submitted_time;
static unsigned confidence;
static int8_t result_input[784];

/* The control mutex serializes user commands, never a worker.  Workers and
 * the interrupt callback exchange state only under the short spinlock.
 * A timeout must not free storage still owned by either worker or DMA. */
static bool stopping(void)
{
  irqstate_t flags = spin_lock_irqsave(&lock);
  bool stop = stop_requested;
  spin_unlock_irqrestore(&lock, flags);
  return stop;
}

static void request_stop(void)
{
  irqstate_t flags = spin_lock_irqsave(&lock);
  stop_requested = true;
  if (video_active) service_state = STOPPING;
  spin_unlock_irqrestore(&lock, flags);
  nxsem_post(&frame_sem);
  nxsem_post(&ai_sem);
}

static void drain(sem_t *sem)
{
  while (nxsem_trywait(sem) == 0);
}

static int release_camera(void)
{
  int ret;
  if (camera_owned)
    {
      ret = esp32s31_camera_dvp_stop();
      if (ret < 0) return ret;
      camera_owned = false;
    }

  if (callback_attached)
    {
      /* A raw callback already admitted on another CPU must retire before
       * its capture slot can be admitted by a subsequent session. */
      clock_t deadline = clock_systime_ticks() + SEC2TICK(1);
      do
        {
          ret = s31vision_camera_raw_callback(NULL);
          if (ret != -EBUSY) break;
          nxsig_usleep(1000);
        }
      while (clock_systime_ticks() < deadline);
      if (ret < 0) return ret;
      callback_attached = false;
    }

  irqstate_t flags = spin_lock_irqsave(&lock);
  int pending = ready_slot;
  ready_slot = -1;
  spin_unlock_irqrestore(&lock, flags);
  if (pending >= 0) s31vision_camera_release(pending);
  return 0;
}

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
  if (stop_requested || index >= 3)
    {
      spin_unlock_irqrestore(&lock, flags);
      s31vision_camera_release(index);
      return;
    }
  bool notify = ready_slot < 0;
  int discard=ready_slot;
  ready_slot=index;captured++;
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
  if(pin(1)<0){ai_error=-errno;nxsem_post(&ai_ready);nxsem_post(&ai_done);return 1;}
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
  ai_error=(errors||correct!=100)?-EILSEQ:0;nxsem_post(&ai_ready);
  if(ai_error){nxsem_post(&ai_done);return 1;}
  int8_t input[784] __attribute__((aligned(16)));
  int previous=-1;unsigned consecutive=0;
  for(;;)
    {
      if(nxsem_wait_uninterruptible(&ai_sem)<0)continue;
      if(stopping())break;
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
  nxsem_post(&ai_done);
  return 0;
}
static int video_main(int argc,char **argv)
{
  (void)argc;(void)argv;
  int ret = 0, cleanup;
  bool ai_created = false, announced = false, lcd_owned = false;
  int current_slot = -1;
  if(pin(0)<0){ret=-errno;goto out;}
  struct fb_vtable_s *v=up_fbgetvplane(0,0);
  struct fb_planeinfo_s plane;
  if(!v){ret=-ENODEV;goto out;}
  ret=v->getplaneinfo(v,0,&plane);
  if(ret<0)goto out;
  uint16_t *fb;
  for(int i=0;i<3;i++)
    {frames[i]=s31vision_camera_buffer(i);if(!frames[i]){ret=-ENODEV;goto out;}}
  if (preview_mode)
    {
      nxmutex_lock(&preview_lock);
      preview_pixels=s31vision_dma_alloc(64,240*240*2);
      preview_sequence=0;
      nxmutex_unlock(&preview_lock);
      if(!preview_pixels){ret=-ENOMEM;goto out;}
      goto camera_start;
    }
  ret=s31vision_lcd_lease(true);
  if(ret<0)goto out;
  lcd_owned=true;
  display_owned=true;
  gray=s31vision_dma_alloc(64,ROI*ROI+280*280);
  if(!gray){ret=-ENOMEM;goto out;}
  ret=kthread_create("vision-ai",100,16384,ai_main,NULL);
  if(ret<0)goto out;
  ai_created=true;
  ret=nxsem_tickwait_uninterruptible(&ai_ready,SEC2TICK(10));
  if(ret<0)goto out;
  if(ai_error){ret=ai_error;goto out;}
  if(stopping()){ret=-ECANCELED;goto out;}
  s31vision_lcd_probe();
  if(stopping()){ret=-ECANCELED;goto out;}
camera_start:
  ret=s31vision_camera_raw_callback(complete);
  if(ret<0)goto out;
  callback_attached=true;
  ret=esp32s31_camera_dvp_start();
  if(ret<0)goto out;
  camera_owned=true;
  irqstate_t startup_flags=spin_lock_irqsave(&lock);
  if (!stop_requested) service_state=RUNNING;
  spin_unlock_irqrestore(&lock,startup_flags);
  announced=true;
  nxsem_post(&video_ready);
  syslog(LOG_INFO,"VISION START cpu=%d VGA=640x480 ROI=200x200 recording=OFF\n",sched_getcpu());
  uint64_t start=now_us(),copy_us=0,roi_us=0,lcd_us=0;unsigned shown=0;char buf[32];
  unsigned display_base=s31vision_lcd_displayed();
  for(;;)
    {
      ret=nxsem_tickwait_uninterruptible(&frame_sem,SEC2TICK(2));
      if(stopping()){ret=0;break;}
      if(ret<0)break;
      irqstate_t flags=spin_lock_irqsave(&lock);
      int index=ready_slot;ready_slot=-1;
      if(index<0){spin_unlock_irqrestore(&lock,flags);continue;}
      current_slot=index;
      if(preview_mode)
        {
          spin_unlock_irqrestore(&lock,flags);
          nxmutex_lock(&preview_lock);
          /* Original vertical correction and RGB565 byte swap, no mirror.
           * Square preview is only a display crop; inference never reads it. */
          for(int y=0;y<240;y++)for(int x=0;x<240;x++)
            {
              uint16_t px=frames[index][(H-1-y*2)*W+80+x*2];
              preview_pixels[y*240+x]=(px>>8)|(px<<8);
            }
          preview_sequence++;
          nxmutex_unlock(&preview_lock);
          s31vision_camera_release(index);
          current_slot=-1;
          continue;
        }
      bool submit=!ai_busy;
      if(submit){ai_busy=true;frame_id=captured;submitted_time=now_us();}
      int digit=now_us()-result_time<1000000?answer:-1;
      unsigned cap=captured,inf=inferred,rid=result_id;
      uint64_t us=infer_us;unsigned conf=confidence;
      int8_t thumbnail[784];memcpy(thumbnail,result_input,sizeof(thumbnail));
      spin_unlock_irqrestore(&lock,flags);
      uint64_t stamp=now_us();
      fb=s31vision_lcd_acquire();
      if(!fb){ret=-EIO;break;}
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
      current_slot=-1;
      for(int y=0;y<H;y++)memset(fb+y*800+W,0,(800-W)*2);
      for(int x=220;x<420;x++){fb[140*800+x]=0x07e0;fb[339*800+x]=0x07e0;}
      for(int y=140;y<340;y++){fb[y*800+220]=0x07e0;fb[y*800+419]=0x07e0;}
      text(fb,650,20,"VISION AI",1);text(fb,650,45,"CPU1 SIMD",1);
      buf[0]=digit<0?'-':'0'+digit;buf[1]=0;text(fb,670,90,buf,8);
      uint64_t elapsed=now_us()-start;
      unsigned display_count=s31vision_lcd_displayed()-display_base;
      snprintf(buf,sizeof(buf),"FPS %u.%u",elapsed?(unsigned)(display_count*10000000ull/elapsed)/10:0,
               elapsed?(unsigned)(display_count*10000000ull/elapsed)%10:0);text(fb,650,240,buf,1);
      snprintf(buf,sizeof(buf),"AI %llu us",(unsigned long long)us);text(fb,650,270,buf,1);
      snprintf(buf,sizeof(buf),"CONF %u%%",digit<0?0:conf);text(fb,650,290,buf,1);
      for(int y=0;y<28;y++)for(int x=0;x<28;x++)
        {
          unsigned g=(unsigned)thumbnail[y*28+x]*255/127;
          uint16_t color=((g>>3)<<11)|((g>>2)<<5)|(g>>3);
          for(int dy=0;dy<4;dy++)for(int dx=0;dx<4;dx++)
            fb[(312+y*4+dy)*800+660+x*4+dx]=color;
        }
      for(int y=438;y<478;y++)for(int x=646;x<794;x++)fb[y*800+x]=0x2104;
      text(fb,670,450,"RETURN",2);
      stamp=now_us();ret=s31vision_lcd_present();lcd_us+=now_us()-stamp;
      if(ret<0)break;
      shown++;
      flags=spin_lock_irqsave(&lock);
      displayed=shown;
      spin_unlock_irqrestore(&lock,flags);
      if(shown%60==0)
        {
        syslog(LOG_INFO,"VISION METRIC cpu=%d captured=%u queued=%u displayed=%u elapsed_us=%llu infer=%u result_frame=%u digit=%d\n",
          sched_getcpu(),cap,shown,s31vision_lcd_displayed()-display_base,(unsigned long long)(now_us()-start),inf,rid,digit);
        s31vision_camera_stats();
        if(shown==60)s31vision_sensor_stats();
        syslog(LOG_INFO,"VISION STAGES copy_us=%llu roi_us=%llu lcd_us=%llu\n",
          (unsigned long long)copy_us,(unsigned long long)roi_us,(unsigned long long)lcd_us);
        }
    }
out:
  request_stop();
  cleanup=release_camera();
  if(ret>=0 && cleanup<0)ret=cleanup;
  if(current_slot>=0)s31vision_camera_release(current_slot);
  if(ai_created)
    {
      /* Cooperative acknowledgement: no AI access to gray after ai_done.
       * If inference is stuck, the controller times out and keeps this
       * session busy; it never frees gray or starts an overlapping one. */
      nxsem_wait_uninterruptible(&ai_done);
    }
  s31vision_dma_free(gray);
  gray=NULL;
  nxmutex_lock(&preview_lock);
  s31vision_dma_free(preview_pixels);
  preview_pixels=NULL;
  nxmutex_unlock(&preview_lock);
  if(lcd_owned)
    {
      cleanup=s31vision_lcd_lease(false);
      if(cleanup>=0)display_owned=false;
      if(ret>=0 && cleanup<0)ret=cleanup;
    }
  syslog(LOG_INFO,"VISION EXIT error=%d\n",ret);
  irqstate_t endflags=spin_lock_irqsave(&lock);
  ai_busy=false;
  if(ret<0 && last_error==0)last_error=ret;
  service_state=last_error<0?FAULT:STOPPED;
  if(!announced)nxsem_post(&video_ready);
  nxsem_post(&video_done);
  video_active=false;
  /* Publish completion tokens and inactivity together.  No late token from
   * this session may be consumed by the next start. */
  spin_unlock_irqrestore(&lock,endflags);
  return ret<0?1:0;
}
int s31vision_start(void)
{
  int ret=nxmutex_lock(&control);
  if(ret<0)return ret;
  irqstate_t flags=spin_lock_irqsave(&lock);
  bool active=video_active;
  bool running=service_state==RUNNING;
  spin_unlock_irqrestore(&lock,flags);
  if(active || camera_owned || callback_attached || display_owned)
    {
      nxmutex_unlock(&control);
      return running?0:-EBUSY;
    }
  drain(&frame_sem);drain(&ai_sem);drain(&ai_ready);drain(&ai_done);
  drain(&video_ready);drain(&video_done);
  flags=spin_lock_irqsave(&lock);
  stop_requested=false;video_active=true;service_state=STARTING;
  last_error=0;ai_error=0;ready_slot=-1;ai_busy=false;
  captured=inferred=displayed=frame_id=result_id=confidence=0;
  infer_us=result_time=submitted_time=0;answer=-1;
  memset(result_input,0,sizeof(result_input));
  spin_unlock_irqrestore(&lock,flags);
  ret=kthread_create("vision-video",110,8192,video_main,NULL);
  if(ret<0)
    {
      flags=spin_lock_irqsave(&lock);
      last_error=ret;video_active=false;stop_requested=true;service_state=FAULT;
      spin_unlock_irqrestore(&lock,flags);
    }
  else
    {
      ret=nxsem_tickwait_uninterruptible(&video_ready,SEC2TICK(15));
      if(ret<0)
        {
          flags=spin_lock_irqsave(&lock);
          if(last_error==0)last_error=ret;
          spin_unlock_irqrestore(&lock,flags);
          request_stop();
          nxsem_tickwait_uninterruptible(&video_done,SEC2TICK(5));
        }
      else
        {
          flags=spin_lock_irqsave(&lock);
          ret=last_error<0?last_error:service_state==RUNNING?0:-ECANCELED;
          spin_unlock_irqrestore(&lock,flags);
        }
    }
  nxmutex_unlock(&control);
  return ret;
}

int s31vision_stop(void)
{
  int ret=nxmutex_lock(&control);
  if(ret<0)return ret;
  irqstate_t flags=spin_lock_irqsave(&lock);
  bool active=video_active;
  spin_unlock_irqrestore(&lock,flags);
  request_stop();
  if(active)
    {
      ret=nxsem_tickwait_uninterruptible(&video_done,SEC2TICK(5));
      if(ret==0 && (camera_owned || callback_attached))ret=-EIO;
    }
  else
    {
      ret=release_camera();
    }
  if(ret==0 && display_owned)
    {
      ret=s31vision_lcd_lease(false);
      if(ret>=0)display_owned=false;
    }
  flags=spin_lock_irqsave(&lock);
  if(ret<0)last_error=ret;
  if(!video_active)service_state=ret<0?FAULT:STOPPED;
  spin_unlock_irqrestore(&lock,flags);
  nxmutex_unlock(&control);
  return ret;
}

struct vision_reader_s
{
  size_t length;
  char text[256];
  unsigned sequence;
};

static int vision_open(struct file *filep)
{
  filep->f_priv=kmm_zalloc(sizeof(struct vision_reader_s));
  if(!filep->f_priv)return -ENOMEM;
  if((filep->f_oflags & O_ACCMODE)!=O_RDONLY)
    {
      nxmutex_lock(&owner_lock);
      if(owner)
        {
          nxmutex_unlock(&owner_lock);
          kmm_free(filep->f_priv);
          return -EBUSY;
        }
      owner=filep->f_priv;
      nxmutex_unlock(&owner_lock);
    }
  return 0;
}

static int vision_close(struct file *filep)
{
  nxmutex_lock(&owner_lock);
  int ret=0;
  if(owner==filep->f_priv)
    {
      ret=s31vision_stop();
      /* On uncertain retirement keep the lease unavailable, not reusable. */
      owner=ret<0?(void *)-1:NULL;
    }
  nxmutex_unlock(&owner_lock);
  kmm_free(filep->f_priv);
  return ret;
}

static ssize_t vision_read(struct file *filep,char *buffer,size_t length)
{
  static const char * const names[]={"stopped","starting","running","stopping","fault"};
  struct vision_reader_s *reader=filep->f_priv;
  if(length==0)return 0;
  if(reader->length==0)
    {
      irqstate_t flags=spin_lock_irqsave(&lock);
      unsigned status=service_state,cap=captured,inf=inferred,shown=displayed;
      int error=last_error,digit=answer;
      bool active=video_active;
      spin_unlock_irqrestore(&lock,flags);
      reader->length=snprintf(reader->text,sizeof(reader->text),
        "state=%s error=%d active=%u captured=%u displayed=%u inferred=%u digit=%d recording=off\n",
        names[status],error,active,cap,shown,inf,digit);
    }
  if((size_t)filep->f_pos>=reader->length)return 0;
  size_t available=reader->length-filep->f_pos;
  if(length>available)length=available;
  memcpy(buffer,reader->text+filep->f_pos,length);
  filep->f_pos+=length;
  return length;
}

static ssize_t vision_write(struct file *filep,const char *buffer,size_t length)
{
  size_t command_length=length;
  if(command_length && buffer[command_length-1]=='\n')command_length--;
  nxmutex_lock(&owner_lock);
  if(owner!=filep->f_priv){nxmutex_unlock(&owner_lock);return -EPERM;}
  int ret;
  if((command_length==5 && memcmp(buffer,"start",5)==0) ||
     (command_length==7 && memcmp(buffer,"preview",7)==0))
    {
      ret=s31vision_stop();
      if(!ret)
        {
          preview_mode=command_length==7;
          ret=s31vision_start();
        }
    }
  else if(command_length==4 && memcmp(buffer,"stop",4)==0)ret=s31vision_stop();
  else ret=-EINVAL;
  nxmutex_unlock(&owner_lock);
  if(ret<0)return ret;
  ((struct vision_reader_s *)filep->f_priv)->length=0;
  filep->f_pos=0;
  return length;
}

static const struct file_operations vision_ops=
{
  .open=vision_open,
  .close=vision_close,
  .read=vision_read,
  .write=vision_write
};

/* One complete native RGB565 240x240 preview per read. Nonblocking reads
 * return EAGAIN until a newer frame exists. Copies run in the caller's address
 * environment; DMA only ever owns the kernel capture slots. */
static ssize_t preview_read(struct file *filep,char *buffer,size_t length)
{
  struct vision_reader_s *reader=filep->f_priv;
  if(length<240*240*2)return -EMSGSIZE;
  nxmutex_lock(&preview_lock);
  ssize_t ret=-EAGAIN;
  if(preview_pixels && preview_sequence && reader->sequence!=preview_sequence)
    {
      memcpy(buffer,preview_pixels,240*240*2);
      reader->sequence=preview_sequence;
      ret=240*240*2;
    }
  nxmutex_unlock(&preview_lock);
  return ret;
}
static const struct file_operations preview_ops=
{
  .open=vision_open,
  .close=vision_close,
  .read=preview_read
};
int s31vision_initialize(void)
{
  int ret=register_driver("/dev/vision",&vision_ops,0660,NULL);
  if(ret<0)return ret;
  ret=register_driver("/dev/vision-frame",&preview_ops,0440,NULL);
  if(ret<0)unregister_driver("/dev/vision");
  return ret; /* Dormant: Factory owns startup and shutdown. */
}
