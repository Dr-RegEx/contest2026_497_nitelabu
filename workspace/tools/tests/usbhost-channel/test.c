/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "esp32s31_usbhost_channel.h"
static uint32_t regs[1024];
static uint32_t last_clear;
static uint32_t rd(void *arg, uint32_t offset)
{
  (void)arg;
  return regs[offset / 4];
}
static void wr(void *arg, uint32_t offset, uint32_t value)
{
  (void)arg;
  if (offset == ESP32S31_OTG_HCINT_OFFSET(0))
    {
      last_clear = value;
      regs[offset / 4] &= ~value;
    }
  else
    regs[offset / 4] = value;
}
static const struct s31_hc_io_s io = {NULL, rd, wr};
static struct s31_hc_transfer_s x;
static void start(void)
{
  memset(regs, 0, sizeof(regs));
  memset(&x, 0, sizeof(x));
  assert(s31_hc_arm(&x, &io, 0, 64) == 0);
  regs[ESP32S31_OTG_HCCHAR_OFFSET(0)/4] = 1u << 31;
}
static bool irq(uint32_t events, uint32_t remaining)
{
  regs[ESP32S31_OTG_HCINT_OFFSET(0)/4] |= events;
  regs[ESP32S31_OTG_HCTSIZ_OFFSET(0)/4] = remaining;
  if (events & OTG_HCINT_CHH)
    regs[ESP32S31_OTG_HCCHAR_OFFSET(0)/4] &= ~(1u << 31);
  return s31_hc_interrupt(&x, &io, 0);
}
int main(void)
{
  start();
  assert(irq(OTG_HCINT_XFRC | OTG_HCINT_CHH, 12));
  assert(x.result == 0 && x.transferred == 52 && x.phase == S31_HC_DONE);
  assert(!irq(OTG_HCINT_XFRC | OTG_HCINT_CHH, 0));
  assert(x.transferred == 52);
  assert(s31_hc_arm(&x,&io,0,64) == 0);
  assert(regs[ESP32S31_OTG_HCINT_OFFSET(0)/4] == 0);
  assert(!s31_hc_interrupt(&x,&io,0));
  start();
  assert(!irq(OTG_HCINT_XFRC, 0));
  assert(x.phase == S31_HC_STOPPING);
  assert(s31_hc_arm(&x,&io,0,64) == -EBUSY);
  assert(irq(OTG_HCINT_CHH,0) && x.result == 0);
  start();assert(!irq(OTG_HCINT_XFRC,0));
  assert(irq(OTG_HCINT_STALL|OTG_HCINT_CHH,0));assert(x.result==-EPIPE);
  const uint32_t errors[] = {OTG_HCINT_STALL,S31_HC_AHBERR,OTG_HCINT_BBERR,
    OTG_HCINT_TXERR,OTG_HCINT_DTERR,OTG_HCINT_FRMOR,OTG_HCINT_NAK,OTG_HCINT_NYET};
  const int results[] = {-EPIPE,-EIO,-EOVERFLOW,-EPROTO,-EPROTO,-EXDEV,-EAGAIN,-EAGAIN};
  for (unsigned int i=0;i<sizeof(errors)/sizeof(errors[0]);i++)
    {
      start();assert(!irq(errors[i],64));
      assert(irq(OTG_HCINT_CHH,64));assert(x.result==results[i]);
    }
  start();assert(irq(OTG_HCINT_STALL|OTG_HCINT_XFRC|OTG_HCINT_CHH,0));
  assert(x.result==-EPIPE);
  start();assert(irq(OTG_HCINT_XFRC|OTG_HCINT_NAK|OTG_HCINT_CHH,0));
  assert(x.result==0);
  for (int reason=-ETIMEDOUT;;reason=-ECANCELED)
    {
      start();assert(s31_hc_stop(&x,&io,0,reason)==0);
      assert(x.phase==S31_HC_STOPPING);
      assert(s31_hc_arm(&x,&io,0,64)==-EBUSY);
      assert(irq(OTG_HCINT_XFRC|OTG_HCINT_CHH,0));assert(x.result==reason);
      if(reason==-ECANCELED)break;
    }
  start();assert(irq(OTG_HCINT_CHH,64));assert(x.result==-EIO);
  start();assert(irq(OTG_HCINT_XFRC|OTG_HCINT_CHH,65));assert(x.result==-EIO);
  start();assert(!irq(OTG_HCINT_ACK|(1u<<31),64));
  assert(last_clear==OTG_HCINT_ACK && x.phase==S31_HC_ACTIVE);
  assert(regs[ESP32S31_OTG_HCINT_OFFSET(0)/4]==(1u<<31));
  assert(s31_hc_arm(&x,&io,16,64)==-EINVAL);
  assert(s31_hc_arm(&x,&io,0,0x80000)==-EINVAL);
  assert(s31_hc_stop(&x,&io,0,-EIO)==-EINVAL);
  puts("DWC2 channel state / W1C / errors / retry / halt / timeout / stale IRQ: PASS");
  return 0;
}
