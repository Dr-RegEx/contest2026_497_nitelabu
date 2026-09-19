/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "esp32s31_usbhost_pio.h"
#include "esp32s31_usbhost_transfer.h"
#include "esp32s31_usbhost_port.h"
static uint32_t r[2048], words[128];
static unsigned int count, ticks, launches;
static int mode;
static bool running, in, ping_needed;
static unsigned int ping_launches;
static struct s31_hc_transfer_s transfer;
static uint32_t rd(void *unused, uint32_t off)
{
  (void)unused;
  if(off==ESP32S31_OTG_GRXSTSP_OFFSET)
    r[ESP32S31_OTG_GINTSTS_OFFSET/4]&=~OTG_GINT_RXFLVL;
  if(off==ESP32S31_OTG_DFIFO_HCH_OFFSET(0)) return 0x00332211;
  return r[off/4];
}
static void wr(void *unused,uint32_t off,uint32_t val)
{
  (void)unused;
  if(off==ESP32S31_OTG_HCINT_OFFSET(0)){r[off/4]&=~val;return;}
  if(off==ESP32S31_OTG_GRSTCTL_OFFSET){r[off/4]=0;return;}
  if(off==ESP32S31_OTG_DFIFO_HCH_OFFSET(0)){words[count++]=val;return;}
  r[off/4]=val;
  if(off==ESP32S31_OTG_HCCHAR_OFFSET(0)&&(val&(1u<<31)))
    {
      if(val&(1u<<30))
        {
          if(mode!=3){r[ESP32S31_OTG_HCINT_OFFSET(0)/4]|=OTG_HCINT_CHH;r[off/4]&=~(1u<<31);}
        }
      else {running=true;ticks=0;launches++;in=(val&OTG_HCCHAR_EPDIR_IN)!=0;}
    }
}
static void delay(void *unused,unsigned int us)
{
  (void)unused;assert(us==100);
  if(!running||mode==3)return;
  if (mode >= 4)
    {
      bool ping = (r[ESP32S31_OTG_HCTSIZ_OFFSET(0)/4] & (1u<<31)) != 0;
      running=false;
      if (ping) ping_launches++;
      uint32_t irq;
      if (mode==6 && ping) irq=OTG_HCINT_STALL;
      else if (mode==7 && ping) {running=true;return;}
      else if (ping) irq=ping_launches==1?OTG_HCINT_NAK:OTG_HCINT_ACK;
      else if (launches==1) irq=mode==5?OTG_HCINT_NAK:
        mode==8?(OTG_HCINT_XFRC|OTG_HCINT_NYET):
        mode==9?(OTG_HCINT_STALL|OTG_HCINT_NYET):OTG_HCINT_NYET;
      else irq=OTG_HCINT_XFRC;
      r[ESP32S31_OTG_HCINT_OFFSET(0)/4]|=irq|OTG_HCINT_CHH;
      r[ESP32S31_OTG_HCCHAR_OFFSET(0)/4]&=~(1u<<31);
      return;
    }
  ticks++;
  if(ticks==1&&in&&mode!=2)
    {
      r[ESP32S31_OTG_GINTSTS_OFFSET/4]|=OTG_GINT_RXFLVL;
      r[ESP32S31_OTG_GRXSTSP_OFFSET/4]=OTG_GRXSTSH_PKTSTS_INRECVD|(3<<4);
    }
  if(ticks>=2)
    {
      running=false;
      r[ESP32S31_OTG_HCINT_OFFSET(0)/4]|= mode==2?OTG_HCINT_STALL:
        (mode==1&&launches==1?OTG_HCINT_NAK:OTG_HCINT_XFRC);
      r[ESP32S31_OTG_HCINT_OFFSET(0)/4]|=OTG_HCINT_CHH;
      r[ESP32S31_OTG_HCCHAR_OFFSET(0)/4]&=~(1u<<31);
      if (in) r[ESP32S31_OTG_HCTSIZ_OFFSET(0)/4]=61;
      /* Slave OUT retains HCTSIZ: success must not require residual zero. */
    }
}
static struct s31_usb_pio_s p;
static void setup(int how)
{
 memset(r,0,sizeof(r));memset(&transfer,0,sizeof(transfer));
 memset(&p,0,sizeof(p));mode=how;count=launches=ping_launches=0;running=false;ping_needed=false;
 p.io=(struct s31_hc_io_s){NULL,rd,wr};p.delay=delay;p.xfer=&transfer;p.maxpacket=64;p.budget=20;p.need_ping=&ping_needed;p.highspeed=how>=4;
 r[ESP32S31_OTG_GINTSTS_OFFSET/4]=OTG_GINTSTS_CMOD;
 r[ESP32S31_OTG_HPRT_OFFSET/4]=OTG_HPRT_PCSTS|OTG_HPRT_PENA;
 r[ESP32S31_OTG_HNPTXSTS_OFFSET/4]=0x100ff;
}
static unsigned int stage;
static unsigned int fault_stage=99;
static ssize_t fault_ret;
static bool dirs[8];static uint8_t pids[8];static size_t lengths[8];
static ssize_t packet(void *arg,bool dir,uint8_t pid,uint8_t *data,size_t len)
{
 (void)arg;(void)data;dirs[stage]=dir;pids[stage]=pid;lengths[stage]=len;stage++;
 return stage-1==fault_stage?fault_ret:(ssize_t)len;
}
static unsigned int port_ms, port_mode;
static uint32_t port_value, port_last;
static uint32_t port_read(void *arg, uint32_t off)
{
 (void)arg; assert(off == ESP32S31_OTG_HPRT_OFFSET); return port_value;
}
static void port_write(void *arg, uint32_t off, uint32_t v)
{
 (void)arg; assert(off == ESP32S31_OTG_HPRT_OFFSET);
 assert(!(v & (OTG_HPRT_PCDET|OTG_HPRT_PENCHNG|OTG_HPRT_POCCHNG|OTG_HPRT_PENA)));
 port_last=v;
}
static void port_delay(unsigned int ms)
{
 port_ms+=ms;
 if (port_mode==1 && port_ms>=60) port_value |= OTG_HPRT_PENA;
 if (port_mode==2 && port_ms>=60) port_value &= ~OTG_HPRT_PCSTS;
}
static void test_port(void)
{
 struct s31_hc_io_s io={NULL,port_read,port_write}; unsigned int stable=0;
 for (unsigned int i=0;i<9;i++) assert(!s31_port_observe(OTG_HPRT_PCSTS,&stable));
 assert(!s31_port_observe(0,&stable)&&stable==0);
 for (unsigned int i=0;i<9;i++) assert(!s31_port_observe(OTG_HPRT_PCSTS,&stable));
 assert(s31_port_observe(OTG_HPRT_PCSTS,&stable));
 assert(!s31_port_observe(OTG_HPRT_PCSTS|OTG_HPRT_POCA,&stable));
 for (unsigned int speed=0;speed<3;speed++) {
 port_mode=1;port_ms=0;port_value=OTG_HPRT_PCSTS|OTG_HPRT_PPWR|OTG_HPRT_PCDET|OTG_HPRT_PENCHNG|(speed<<17);
 assert(s31_port_reset(&io,port_delay)==(int)speed);
 assert(port_ms>=60&&!(port_last&OTG_HPRT_PRST)&&(port_last&OTG_HPRT_PPWR)); }
 port_mode=0;port_ms=0;port_value=OTG_HPRT_PCSTS;
 assert(s31_port_reset(&io,port_delay)==-ETIMEDOUT&&port_ms==1050);
 port_mode=2;port_ms=0;port_value=OTG_HPRT_PCSTS;
 assert(s31_port_reset(&io,port_delay)==-ENODEV);
 port_mode=1;port_ms=0;port_value=OTG_HPRT_PCSTS|(3u<<17);
 assert(s31_port_reset(&io,port_delay)==-EPROTO);
}
int main(void)
{
 test_port();
 uint8_t b[100]={0xaa,1,2,3,4,5,0xbb};uint8_t req[8]={0x80,6,0,0,0,0,100,0};
 setup(4);assert(s31_usb_pio_packet(&p,false,0,b+1,5)==5);
 assert(ping_needed&&launches==1&&count==2); /* NYET accepted, never replay. */
 assert(s31_usb_pio_packet(&p,false,2,b+1,5)==5);
 assert(!ping_needed&&ping_launches==2&&launches==4&&count==4);
 setup(8);assert(s31_usb_pio_packet(&p,false,0,b+1,5)==5&&ping_needed&&launches==1);
 setup(9);assert(s31_usb_pio_packet(&p,false,0,b+1,5)==-EPIPE);
 setup(5);assert(s31_usb_pio_packet(&p,false,0,b+1,5)==5);
 assert(ping_launches==2&&launches==4&&count==4); /* NAK requires data retry. */
 setup(6);assert(s31_usb_pio_packet(&p,false,0,b+1,5)==5);
 assert(s31_usb_pio_packet(&p,false,2,b+1,5)==-EPIPE&&count==2);
 setup(7);assert(s31_usb_pio_packet(&p,false,0,b+1,5)==5);
 assert(s31_usb_pio_packet(&p,false,2,b+1,5)==-ETIMEDOUT&&count==2);
 setup(0);assert(s31_usb_pio_packet(&p,false,0,b+1,5)==5);
 assert(count==2&&words[0]==0x04030201&&words[1]==5);
 setup(0);memset(b,0xaa,sizeof(b));assert(s31_usb_pio_packet(&p,true,2,b+1,8)==3);
 assert(b[0]==0xaa&&b[1]==0x11&&b[3]==0x33&&b[4]==0xaa);
 setup(0);assert(s31_usb_pio_packet(&p,true,2,b+1,2)==-EOVERFLOW);
 setup(1);assert(s31_usb_pio_packet(&p,false,0,b,5)==5&&launches==2);
 setup(2);assert(s31_usb_pio_packet(&p,false,0,b,5)==-EPIPE);
 setup(3);assert(s31_usb_pio_packet(&p,false,0,b,5)==-ETIMEDOUT);
 assert(transfer.phase==S31_HC_STOPPING);p.budget=20;
 assert(s31_usb_pio_packet(&p,false,0,b,5)==-EBUSY);
 setup(0);r[ESP32S31_OTG_HPRT_OFFSET/4]=0;
 assert(s31_usb_pio_packet(&p,false,0,b,5)==-ENODEV&&launches==0);
 setup(0);r[ESP32S31_OTG_GAHBCFG_OFFSET/4]=1u<<5;
 assert(s31_usb_pio_packet(&p,false,0,b,5)==-ENOTSUP);
 stage=0;assert(s31_usb_control(packet,NULL,64,req,b)==0);
 assert(stage==4&&!dirs[0]&&pids[0]==3&&lengths[0]==8);
 assert(dirs[1]&&pids[1]==2&&lengths[1]==64);
 assert(dirs[2]&&pids[2]==0&&lengths[2]==36);
 assert(!dirs[3]&&pids[3]==2&&lengths[3]==0);
 stage=0;req[0]=0;req[6]=0;assert(s31_usb_control(packet,NULL,64,req,NULL)==0);
 assert(stage==2&&dirs[1]&&pids[1]==2&&lengths[1]==0);
 stage=0;req[0]=0x80;req[6]=100;fault_stage=1;fault_ret=7;
 assert(s31_usb_control(packet,NULL,64,req,b)==0&&stage==3);
 assert(!dirs[2]&&pids[2]==2&&lengths[2]==0);
 stage=0;fault_stage=0;fault_ret=-EPIPE;
 assert(s31_usb_control(packet,NULL,64,req,b)==-EPIPE&&stage==1);
 stage=0;fault_stage=2;fault_ret=-ETIMEDOUT;
 assert(s31_usb_control(packet,NULL,64,req,b)==-ETIMEDOUT&&stage==3);
 stage=0;req[0]=0;fault_stage=1;fault_ret=3;
 assert(s31_usb_control(packet,NULL,64,req,b)==-EPROTO&&stage==2);
 fault_stage=99;stage=0;
 uint8_t big[1024]={0};uint8_t pid=0;
 assert(s31_usb_data(packet,NULL,false,512,&pid,big,sizeof(big))==1024);
 assert(stage==2&&lengths[0]==512&&lengths[1]==512&&pids[0]==0&&pids[1]==2&&pid==0);
 puts("HS NYET/PING/ACK/NAK/error/timeout / root port debounce/reset/speed/W1C / PIO FIFO byte boundaries / RX short+overflow / NAK retry / STALL / timeout / EP0 sequence: PASS");
 return 0;
}
