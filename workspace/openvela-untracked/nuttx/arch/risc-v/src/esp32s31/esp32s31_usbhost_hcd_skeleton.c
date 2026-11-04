/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_usbhost_hcd_skeleton.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/signal.h>
#include <nuttx/kthread.h>
#include <syslog.h>
#include <nuttx/usb/usbhost_devaddr.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/kmalloc.h>
#include <string.h>
#include <nuttx/usb/usb.h>
#include <nuttx/usb/usbhost.h>

#include "riscv_internal.h"
#include "hardware/esp32s31_otg.h"
#include "esp32s31_usbhost_channel.h"
#include "esp32s31_usbhost_transfer.h"
#include "esp32s31_usbhost_pio.h"
#include "esp32s31_usbhost_port.h"
#include "esp32s31_usbhost.h"

#if defined(CONFIG_ESP32S31_USBHOST_HCD_SKELETON) && \
    defined(CONFIG_ESP32S31_USBHOST) && defined(CONFIG_USBHOST)

#if defined(CONFIG_USBHOST_ASYNCH) && defined(CONFIG_SMP)
#  error "S31 USB Host asynchronous callback retirement requires !SMP"
#endif

/* Experimental HCD: serialized polling PIO implements control, bulk and
 * single-transaction interrupt endpoints on the root port. Optional async
 * requests share the FIFO through a dedicated packet-round-robin worker.
 * This polling scheduler does not reserve periodic bandwidth or guarantee
 * microframe response latency. IRQ scheduling, hubs and isochronous remain.
 */

#define ESP32S31_USBHOST_HCD_MAX_CHANNELS 16

enum esp32s31_usbhost_chanstate_e
{
  ESP32S31_USBHOST_CH_RESET = 0,
  ESP32S31_USBHOST_CH_READY,
  ESP32S31_USBHOST_CH_ACTIVE,
  ESP32S31_USBHOST_CH_HALTED
};

struct esp32s31_usbhost_chan_s
{
  uint8_t channel;
  uint8_t funcaddr;
  uint8_t epnum;
  uint8_t xfrtype;
  uint16_t maxpacket;
  bool in;
  bool allocated;
  uint8_t speed;
  uint8_t pid;
  bool need_ping;
  enum esp32s31_usbhost_chanstate_e state;
  uint32_t hcchar;
  uint32_t hctsiz;
  struct s31_hc_transfer_s transfer;
  struct s31_usb_periodic_s periodic;
#ifdef CONFIG_USBHOST_ASYNCH
  struct esp32s31_usbhost_async_s *async;
  struct esp32s31_usbhost_async_s *async_active;
#endif
};

struct esp32s31_usbhost_hcd_s
{
  /* NuttX requires the common driver to be the first member. */
  struct usbhost_driver_s drvr;
  struct usbhost_connection_s conn;
  struct usbhost_roothubport_s rhport;
  struct usbhost_devaddr_s devgen;
  unsigned int stable;
  bool detached;
  struct esp32s31_usbhost_chan_s chan[
    ESP32S31_USBHOST_HCD_MAX_CHANNELS];
};

/* The synchronous caller owns this object until it retires the request.
 * Cancellation never retains its address after releasing g_request_lock.
 */

struct esp32s31_usbhost_request_s
{
  struct esp32s31_usbhost_chan_s *chan;
  bool cancelled;
};

#ifdef CONFIG_USBHOST_ASYNCH
struct esp32s31_usbhost_async_s
{
  struct esp32s31_usbhost_async_s *next;
  struct esp32s31_usbhost_request_s request;
  usbhost_asynch_t callback;
  void *arg;
  uint8_t *buffer;
  size_t length;
  size_t offset;
  uint64_t deadline;
  bool running;
  /* Hardware never accesses the class driver's buffer after cancellation.
   * This storage remains owned by the worker until the packet has stopped.
   */
  uint8_t packet[1024];
};
#endif

_Static_assert(offsetof(struct esp32s31_usbhost_hcd_s, drvr) == 0,
               "NuttX HCD driver must be first");
_Static_assert(ESP32S31_OTG_HCCHAR_OFFSET(1) -
               ESP32S31_OTG_HCCHAR_OFFSET(0) == 0x20,
               "DWC2 channel stride");
_Static_assert(ESP32S31_OTG_HCTSIZ_OFFSET(0) ==
               ESP32S31_OTG_HCCHAR_OFFSET(0) + 0x10,
               "DWC2 transfer-size offset");

static struct esp32s31_usbhost_hcd_s g_esp32s31_usbhost_hcd;
static mutex_t g_pio_lock = NXMUTEX_INITIALIZER;
static spinlock_t g_request_lock = SP_UNLOCKED;
static struct esp32s31_usbhost_request_s *g_request;
static bool g_resetting;
#ifdef CONFIG_USBHOST_ASYNCH
static struct esp32s31_usbhost_async_s *g_async_head;
static struct esp32s31_usbhost_async_s *g_async_tail;
static sem_t g_async_sem = SEM_INITIALIZER(0);
static bool g_async_wakeup;
static bool g_async_started;
#endif

/* Real channel register access; this handler is not attached until the
 * root-hub/FIFO scheduler is implemented. No MMIO is performed by obtaining
 * the skeleton connection or by its offline contract check.
 */

static uint32_t esp32s31_hcd_read(void *arg, uint32_t offset)
{
  UNUSED(arg);
  return getreg32(DR_REG_USB_BASE + offset);
}

static void esp32s31_hcd_write(void *arg, uint32_t offset, uint32_t value)
{
  UNUSED(arg);
  putreg32(value, DR_REG_USB_BASE + offset);
}

static const struct s31_hc_io_s g_channel_io =
{
  .read = esp32s31_hcd_read,
  .write = esp32s31_hcd_write
};

int esp32s31_usbhost_hcd_channel_arm(unsigned int channel,
                                   uint32_t programmed)
{
  irqstate_t flags;
  int ret;

  if (channel >= ESP32S31_USBHOST_HCD_MAX_CHANNELS)
    {
      return -EINVAL;
    }

  flags = enter_critical_section();
  ret = s31_hc_arm(&g_esp32s31_usbhost_hcd.chan[channel].transfer,
                  &g_channel_io, channel, programmed);
  if (ret == 0)
    {
      g_esp32s31_usbhost_hcd.chan[channel].state = ESP32S31_USBHOST_CH_ACTIVE;
    }

  leave_critical_section(flags);
  return ret;
}

int esp32s31_usbhost_hcd_channel_stop(unsigned int channel, int reason)
{
  irqstate_t flags;
  int ret;

  if (channel >= ESP32S31_USBHOST_HCD_MAX_CHANNELS)
    {
      return -EINVAL;
    }

  flags = enter_critical_section();
  ret = s31_hc_stop(&g_esp32s31_usbhost_hcd.chan[channel].transfer,
                   &g_channel_io, channel, reason);
  leave_critical_section(flags);
  return ret;
}

int esp32s31_usbhost_hcd_channel_interrupt(int irq, void *context, void *arg)
{
  irqstate_t flags;
  uint32_t channels;
  unsigned int channel;

  UNUSED(irq);
  UNUSED(context);
  UNUSED(arg);
  flags = enter_critical_section();
  channels = getreg32(ESP32S31_OTG_HAINT) &
             getreg32(ESP32S31_OTG_HAINTMSK);
  for (channel = 0; channel < ESP32S31_USBHOST_HCD_MAX_CHANNELS; channel++)
    {
      if (channels & (1u << channel))
        {
          struct esp32s31_usbhost_chan_s *chan =
            &g_esp32s31_usbhost_hcd.chan[channel];
          if (s31_hc_interrupt(&chan->transfer, &g_channel_io, channel))
            {
              chan->state = ESP32S31_USBHOST_CH_HALTED;
            }
        }
    }

  leave_critical_section(flags);
  return 0;
}

static uint32_t esp32s31_hcd_hcchar(const struct esp32s31_usbhost_chan_s *chan)
{
  uint32_t value;

  value = ((uint32_t)chan->maxpacket << OTG_HCCHAR_MPSIZ_SHIFT) &
          OTG_HCCHAR_MPSIZ_MASK;
  value |= ((uint32_t)chan->epnum << OTG_HCCHAR_EPNUM_SHIFT) &
           OTG_HCCHAR_EPNUM_MASK;
  value |= ((uint32_t)chan->xfrtype << OTG_HCCHAR_EPTYP_SHIFT) &
           OTG_HCCHAR_EPTYP_MASK;
  value |= ((uint32_t)chan->funcaddr << OTG_HCCHAR_DAD_SHIFT) &
           OTG_HCCHAR_DAD_MASK;
  if (chan->in)
    {
      value |= OTG_HCCHAR_EPDIR_IN;
    }

  return value;
}

static uint32_t esp32s31_hcd_hctsiz(size_t buflen, uint16_t packets,
                                    uint8_t pid)
{
  uint32_t value;

  value = ((uint32_t)buflen << OTG_HCTSIZ_XFRSIZ_SHIFT) &
          OTG_HCTSIZ_XFRSIZ_MASK;
  value |= ((uint32_t)packets << OTG_HCTSIZ_PKTCNT_SHIFT) &
           OTG_HCTSIZ_PKTCNT_MASK;
  value |= ((uint32_t)pid << OTG_HCTSIZ_DPID_SHIFT) &
           OTG_HCTSIZ_DPID_MASK;
  return value;
}

static void esp32s31_hcd_channel_prepare(
  struct esp32s31_usbhost_chan_s *chan)
{
  chan->hcchar = esp32s31_hcd_hcchar(chan);
  chan->hctsiz = esp32s31_hcd_hctsiz(0, 1, OTG_PID_DATA0);
  chan->state = ESP32S31_USBHOST_CH_READY;
}

static struct esp32s31_usbhost_chan_s *esp32s31_hcd_endpoint(usbhost_ep_t ep)
{
  unsigned int i;
  for (i = 0; i < ESP32S31_USBHOST_HCD_MAX_CHANNELS; i++)
    if (ep == &g_esp32s31_usbhost_hcd.chan[i] &&
        g_esp32s31_usbhost_hcd.chan[i].allocated)
      return &g_esp32s31_usbhost_hcd.chan[i];
  return NULL;
}

static bool esp32s31_hcd_cancelled(void *arg)
{
  struct esp32s31_usbhost_request_s *request = arg;
  irqstate_t flags = spin_lock_irqsave(&g_request_lock);
  bool cancelled = request->cancelled;
  spin_unlock_irqrestore(&g_request_lock, flags);
  return cancelled;
}

static ssize_t esp32s31_hcd_finish(
  struct esp32s31_usbhost_request_s *request, ssize_t result)
{
  irqstate_t flags = spin_lock_irqsave(&g_request_lock);

  /* Retirement and cancel have one ordering point. A cancel published
   * before retirement wins; a later cancel cannot touch the caller's stack.
   * A cancelled request can already have transferred packets to the device.
   */

  DEBUGASSERT(g_request == request);
  if (request->cancelled)
    result = -ESHUTDOWN; /* Public DRVR_CANCEL completion contract. */
  g_request = NULL;
  spin_unlock_irqrestore(&g_request_lock, flags);
  return result;
}

static int esp32s31_hcd_fifo_ready(void)
{
  unsigned int i;

  if (getreg32(DR_REG_USB_BASE + ESP32S31_OTG_GRSTCTL_OFFSET) &
      (OTG_GRSTCTL_RXFFLSH | OTG_GRSTCTL_TXFFLSH))
    return -EBUSY;
  /* FIFO access is global. Never flush or consume another channel's data
   * before its halt acknowledgment, even if its client has already left.
   */
  for (i = 0; i < ESP32S31_USBHOST_HCD_MAX_CHANNELS; i++)
    if (!s31_hc_reusable(&g_esp32s31_usbhost_hcd.chan[i].transfer,
                         &g_channel_io, i))
      return -EBUSY;
  return 0;
}

static int esp32s31_hcd_begin(struct usbhost_driver_s *drvr, usbhost_ep_t ep,
                             bool control,
                             struct esp32s31_usbhost_request_s *request)
{
  struct esp32s31_usbhost_chan_s *chan;
  irqstate_t flags;
  int ret;

  if (drvr != &g_esp32s31_usbhost_hcd.drvr)
    return -EINVAL;
  if (control && ep == NULL)
    ep = &g_esp32s31_usbhost_hcd.chan[0];
  flags = spin_lock_irqsave(&g_request_lock);
  chan = esp32s31_hcd_endpoint(ep);
  if (!chan)
    ret = -EINVAL;
  else if (control ? chan->xfrtype != USB_EP_ATTR_XFER_CONTROL :
           (chan->xfrtype != USB_EP_ATTR_XFER_BULK &&
            chan->xfrtype != USB_EP_ATTR_XFER_INT))
    ret = -ENOTSUP;
  else if (g_request || g_resetting)
    ret = -EBUSY;
#ifdef CONFIG_USBHOST_ASYNCH
  else if (chan->async || chan->async_active)
    ret = -EBUSY;
#endif
  else
    {
      request->chan = chan;
      request->cancelled = false;
      g_request = request;
      ret = 0;
    }
  spin_unlock_irqrestore(&g_request_lock, flags);
  if (ret < 0)
    return ret;

  /* Publish before waiting for the FIFO mutex, so even a request waiting
   * behind endpoint administration can be cancelled. The reservation pins
   * the endpoint and rejects a second synchronous request (usbhost.h).
   */

  ret = nxmutex_lock(&g_pio_lock);
  if (ret < 0)
    return esp32s31_hcd_finish(request, ret);
  if (esp32s31_hcd_cancelled(request))
    ret = -ECANCELED;
  else
    ret = esp32s31_hcd_fifo_ready();
  if (ret < 0)
    {
      ret = esp32s31_hcd_finish(request, ret);
      nxmutex_unlock(&g_pio_lock);
    }
  return ret;
}

static bool esp32s31_hcd_packet_valid(uint8_t speed, uint8_t type, uint16_t mps)
{
#ifndef CONFIG_USBHOST_INT_DISABLE
  if (type == USB_EP_ATTR_XFER_INT)
    {
      /* Encoded HS additional-transaction bits are deliberately rejected.
       * This engine handles one transaction per endpoint service period.
       */
      return mps && ((speed == USB_SPEED_HIGH && mps <= 1024) ||
                     (speed == USB_SPEED_FULL && mps <= 64) ||
                     (speed == USB_SPEED_LOW && mps <= 8));
    }
#endif
  if (type != USB_EP_ATTR_XFER_CONTROL && type != USB_EP_ATTR_XFER_BULK)
    return false;
  if (speed == USB_SPEED_HIGH)
    return mps == (type == USB_EP_ATTR_XFER_CONTROL ? 64 : 512);
  if (speed == USB_SPEED_LOW)
    return type == USB_EP_ATTR_XFER_CONTROL && mps == 8;
  return speed == USB_SPEED_FULL &&
         (mps == 8 || mps == 16 || mps == 32 || mps == 64);
}

static void esp32s31_hcd_delay(void *arg, unsigned int us)
{
  UNUSED(arg);
  up_udelay(us);
}

static uint64_t esp32s31_hcd_now_ms(void)
{
  return TICK2MSEC((uint64_t)clock_systime_ticks());
}

static void esp32s31_hcd_pio_setup(struct s31_usb_pio_s *pio,
                                  struct esp32s31_usbhost_request_s *request)
{
  struct esp32s31_usbhost_chan_s *chan = request->chan;
  memset(pio, 0, sizeof(*pio));
  pio->io = g_channel_io;
  pio->xfer = &chan->transfer;
  pio->delay = esp32s31_hcd_delay;
  pio->maxpacket = chan->maxpacket;
  pio->highspeed = chan->speed == USB_SPEED_HIGH;
  pio->periodic = chan->xfrtype == USB_EP_ATTR_XFER_INT;
  pio->yield_nak = pio->periodic;
  pio->need_ping = &chan->need_ping;
  pio->cancelled = esp32s31_hcd_cancelled;
  pio->cancel_arg = request;
  pio->channel = chan->channel;
  /* A flash drive can NAK its CSW while programming media. The generic
   * busy-loop delay is not a clock: use elapsed system time for the whole
   * request, including retries, rather than counting nominal 100-us waits.
   */
  pio->budget = UINT32_MAX;
  pio->now_ms = esp32s31_hcd_now_ms;
  pio->deadline_ms = pio->now_ms() + 5000;
  pio->hcchar = esp32s31_hcd_hcchar(chan) | (1u << OTG_HCCHAR_MCNT_SHIFT);
  if (chan->speed == USB_SPEED_LOW)
    pio->hcchar |= OTG_HCCHAR_LSDEV;
}

static bool esp32s31_hcd_periodic_due(struct esp32s31_usbhost_chan_s *chan)
{
  return s31_usb_periodic_due(&chan->periodic,
    getreg32(ESP32S31_OTG_HFNUM), esp32s31_hcd_now_ms(),
    chan->speed == USB_SPEED_HIGH, (CONFIG_USEC_PER_TICK + 999) / 1000);
}

static void esp32s31_hcd_periodic_record(struct esp32s31_usbhost_chan_s *chan,
                                        const struct s31_usb_pio_s *pio)
{
  if (pio->periodic && pio->launched)
    s31_usb_periodic_record(&chan->periodic, pio->launch_frame, pio->launch_ms);
}

static void esp32s31_port_delay(unsigned int ms)
{
  nxsig_usleep(ms * 1000);
}

static int esp32s31_hcd_ep0configure(struct usbhost_driver_s *drvr,
                                    usbhost_ep_t ep0, uint8_t funcaddr,
                                    uint8_t speed, uint16_t maxpacketsize);

static int esp32s31_hcd_wait(struct usbhost_connection_s *conn,
                             struct usbhost_hubport_s **hport)
{
  struct esp32s31_usbhost_hcd_s *priv = &g_esp32s31_usbhost_hcd;
  struct usbhost_hubport_s *port = &priv->rhport.hport;
  uint32_t value;
  bool connected;
  if (conn != &priv->conn || !hport)
    return -EINVAL;
  *hport = NULL;
  for (;;)
    {
      value = esp32s31_hcd_read(NULL, ESP32S31_OTG_HPRT_OFFSET);
      connected = s31_port_observe(value, &priv->stable);
      if (connected != port->connected)
        {
          port->connected = connected;
          if (!connected && !priv->detached)
            {
              /* A class can notice transfer errors and destroy itself
               * before the root polling task observes the removed cable.
               * The physical disconnect still requires channel recovery.
               */

              priv->detached = true;
              if (port->devclass)
                CLASS_DISCONNECTED(port->devclass);
            }
          *hport = port;
          return 0;
        }
      if (nxsig_usleep(10000) < 0)
        return -EINTR;
    }
}

/* Called with the PIO mutex held and new requests blocked by g_resetting.
 * Unplugging can remove the bus clock before an armed channel reports CHH.
 * Keep that channel pinned during class destruction. Once the root class,
 * address and every software request have gone, a complete core reset can
 * retire the orphaned hardware state before a new device is enumerated.
 */

static int esp32s31_hcd_recover_detached(void)
{
  struct esp32s31_usbhost_hcd_s *priv = &g_esp32s31_usbhost_hcd;
  struct usbhost_hubport_s *hport = &priv->rhport.hport;
  unsigned int i;
  int ret;

  if (!priv->detached)
    return 0;
  if (!g_resetting || hport->devclass || hport->funcaddr || g_request)
    return -EBUSY;
#ifdef CONFIG_USBHOST_ASYNCH
  if (g_async_head || g_async_tail)
    return -EBUSY;
  for (i = 0; i < ESP32S31_USBHOST_HCD_MAX_CHANNELS; i++)
    if (priv->chan[i].async || priv->chan[i].async_active)
      return -EBUSY;
#endif

  ret = esp32s31_usbhost_initialize();
  if (ret < 0)
    return ret;
  for (i = 0; i < ESP32S31_USBHOST_HCD_MAX_CHANNELS; i++)
    if (getreg32(ESP32S31_OTG_HCCHAR(i)) & (1u << 31))
      return -EBUSY;

  /* Successful hardware reset is the retirement proof. A timeout alone
   * still cannot free/reuse a channel, and no class buffer is retained.
   */

  memset(priv->chan, 0, sizeof(priv->chan));
  for (i = 0; i < ESP32S31_USBHOST_HCD_MAX_CHANNELS; i++)
    priv->chan[i].channel = i;
  return 0;
}

static int esp32s31_hcd_enumerate(struct usbhost_connection_s *conn,
                                  struct usbhost_hubport_s *hport)
{
  struct esp32s31_usbhost_hcd_s *priv = &g_esp32s31_usbhost_hcd;
  irqstate_t flags;
  unsigned int i;
  int speed;
  int ret;
  if (conn != &priv->conn || hport != &priv->rhport.hport || !hport->connected)
    return -ENODEV;
  /* A detached class may still own open files and defer destruction. */
  if (hport->devclass || hport->funcaddr)
    return -EBUSY;
  ret = nxmutex_lock(&g_pio_lock);
  if (ret < 0)
    return ret;
  flags = spin_lock_irqsave(&g_request_lock);
  if (g_request)
    {
      spin_unlock_irqrestore(&g_request_lock, flags);
      nxmutex_unlock(&g_pio_lock);
      return -EBUSY;
    }
  g_resetting = true;
  spin_unlock_irqrestore(&g_request_lock, flags);
  ret = esp32s31_hcd_recover_detached();
  if (ret < 0)
    {
      speed = ret;
      goto reset_done;
    }
  for (i = 0; i < ESP32S31_USBHOST_HCD_MAX_CHANNELS; i++)
    if ((i && priv->chan[i].allocated) ||
#ifdef CONFIG_USBHOST_ASYNCH
        priv->chan[i].async || priv->chan[i].async_active ||
#endif
        !s31_hc_reusable(&priv->chan[i].transfer, &g_channel_io, i))
      {
        flags = spin_lock_irqsave(&g_request_lock);
        g_resetting = false;
        spin_unlock_irqrestore(&g_request_lock, flags);
        nxmutex_unlock(&g_pio_lock);
        return -EBUSY;
      }
  speed = s31_port_reset(&g_channel_io, esp32s31_port_delay);
reset_done:
  flags = spin_lock_irqsave(&g_request_lock);
  g_resetting = false;
  spin_unlock_irqrestore(&g_request_lock, flags);
  nxmutex_unlock(&g_pio_lock);
  if (speed < 0)
    return speed;
  hport->speed = speed == 0 ? USB_SPEED_HIGH :
                 speed == 1 ? USB_SPEED_FULL : USB_SPEED_LOW;
  priv->detached = false;
  ret = esp32s31_hcd_ep0configure(&priv->drvr, NULL, 0, hport->speed,
                                 speed == 0 ? 64 : 8);
  if (ret < 0)
    return ret;
  return usbhost_enumerate(hport, &hport->devclass);
}

/* Endpoint and synchronous transfer callbacks share one non-periodic FIFO
 * lock. A channel which has not acknowledged halt cannot be reused. */

static int esp32s31_hcd_ep0configure(struct usbhost_driver_s *drvr,
                                     usbhost_ep_t ep0, uint8_t funcaddr,
                                     uint8_t speed, uint16_t maxpacketsize)
{
  struct esp32s31_usbhost_chan_s *chan;
  irqstate_t flags;
  int ret = nxmutex_lock(&g_pio_lock);
  if (ret < 0)
    return ret;
  flags = spin_lock_irqsave(&g_request_lock);
  chan = ep0 == NULL ? &g_esp32s31_usbhost_hcd.chan[0] :
                       esp32s31_hcd_endpoint(ep0);
  if (!chan || drvr != &g_esp32s31_usbhost_hcd.drvr || funcaddr > 127 ||
      !esp32s31_hcd_packet_valid(speed, USB_EP_ATTR_XFER_CONTROL, maxpacketsize))
    ret = -EINVAL;
  else if ((g_request && g_request->chan == chan) ||
#ifdef CONFIG_USBHOST_ASYNCH
           chan->async || chan->async_active ||
#endif
           !s31_hc_reusable(&chan->transfer, &g_channel_io, chan->channel))
    ret = -EBUSY;
  else
    {
      chan->allocated = true;
      chan->funcaddr = funcaddr;
      chan->epnum = 0;
      chan->xfrtype = USB_EP_ATTR_XFER_CONTROL;
      chan->maxpacket = maxpacketsize;
      chan->speed = speed;
      chan->pid = OTG_PID_DATA0;
      chan->need_ping = false;
      ret = 0;
    }
  spin_unlock_irqrestore(&g_request_lock, flags);
  nxmutex_unlock(&g_pio_lock);
  return ret;
}

static int esp32s31_hcd_epalloc(struct usbhost_driver_s *drvr,
                                const struct usbhost_epdesc_s *epdesc,
                                usbhost_ep_t *ep)
{
  irqstate_t flags;
  unsigned int i;
  int interval = 0;
  int ret;
  if (ep == NULL)
    return -EINVAL;
  *ep = NULL;
  if (drvr != &g_esp32s31_usbhost_hcd.drvr || !epdesc || !epdesc->hport ||
      epdesc->hport->funcaddr > 127 || (epdesc->addr & 0x70) ||
      ((epdesc->xfrtype == USB_EP_ATTR_XFER_BULK ||
        epdesc->xfrtype == USB_EP_ATTR_XFER_INT) &&
       (epdesc->addr & 0x0f) == 0))
    return -EINVAL;
#ifdef CONFIG_USBHOST_HUB
  if (epdesc->hport->parent != NULL)
    return -ENOTSUP; /* Split transactions/hub routing are not implemented. */
#endif
  if (!esp32s31_hcd_packet_valid(epdesc->hport->speed, epdesc->xfrtype,
                                epdesc->mxpacketsize))
    return -ENOTSUP;
  if (epdesc->xfrtype == USB_EP_ATTR_XFER_INT)
    {
      interval = s31_usb_periodic_interval(
        epdesc->hport->speed == USB_SPEED_HIGH, epdesc->interval);
      if (interval < 0)
        return interval;
      if (epdesc->hport->speed == USB_SPEED_LOW && interval < 10)
        interval = 10;
    }
  ret = nxmutex_lock(&g_pio_lock);
  if (ret < 0)
    return ret;
  flags = spin_lock_irqsave(&g_request_lock);
  ret = -ENOMEM;
  for (i = 1; i < ESP32S31_USBHOST_HCD_MAX_CHANNELS; i++)
    if (!g_esp32s31_usbhost_hcd.chan[i].allocated &&
#ifdef CONFIG_USBHOST_ASYNCH
        !g_esp32s31_usbhost_hcd.chan[i].async_active &&
#endif
        s31_hc_reusable(&g_esp32s31_usbhost_hcd.chan[i].transfer,
                        &g_channel_io, i))
      {
        struct esp32s31_usbhost_chan_s *chan = &g_esp32s31_usbhost_hcd.chan[i];
        memset(chan, 0, sizeof(*chan));
        chan->allocated = true;
        chan->channel = i;
        chan->funcaddr = epdesc->hport->funcaddr;
        chan->epnum = epdesc->addr & 0x0f;
        chan->maxpacket = epdesc->mxpacketsize;
        chan->speed = epdesc->hport->speed;
        chan->xfrtype = epdesc->xfrtype;
        chan->in = epdesc->in;
        chan->periodic.interval = interval;
        *ep = chan;
        ret = 0;
        break;
      }
  spin_unlock_irqrestore(&g_request_lock, flags);
  nxmutex_unlock(&g_pio_lock);
  return ret;
}

static int esp32s31_hcd_epfree(struct usbhost_driver_s *drvr,
                               usbhost_ep_t ep)
{
  struct esp32s31_usbhost_chan_s *chan;
  irqstate_t flags;
  int ret;

  /* A class may free its endpoint immediately after cancel, including in
   * a critical section. Do not wait for the polling worker's FIFO mutex.
   * An in-flight cancelled packet pins the physical channel separately.
   */
  flags = spin_lock_irqsave(&g_request_lock);
  chan = esp32s31_hcd_endpoint(ep);
  if (!chan || drvr != &g_esp32s31_usbhost_hcd.drvr)
    ret = -EINVAL;
  else if (g_request && g_request->chan == chan)
    ret = -EBUSY;
#ifdef CONFIG_USBHOST_ASYNCH
  else if (chan->async)
    ret = -EBUSY;
  else if (chan->async_active && chan->async_active->request.cancelled)
    {
      chan->allocated = false;
      ret = 0;
    }
  else if (chan->async_active)
    ret = -EBUSY;
#endif
  else if (!s31_hc_reusable(&chan->transfer, &g_channel_io, chan->channel))
    ret = -EBUSY;
  else
    {
      chan->allocated = false;
      ret = 0;
    }
  spin_unlock_irqrestore(&g_request_lock, flags);
  return ret;
}

static int esp32s31_hcd_alloc(struct usbhost_driver_s *drvr,
                              uint8_t **buffer, size_t *maxlen)
{
  UNUSED(drvr);
  if (!buffer || !maxlen)
    return -EINVAL;
  *buffer = kmm_malloc(512);
  *maxlen = *buffer ? 512 : 0;
  return *buffer ? 0 : -ENOMEM;
}

static int esp32s31_hcd_free(struct usbhost_driver_s *drvr,
                             uint8_t *buffer)
{
  UNUSED(drvr);
  kmm_free(buffer);
  return 0;
}

static int esp32s31_hcd_ioalloc(struct usbhost_driver_s *drvr,
                                uint8_t **buffer, size_t buflen)
{
  UNUSED(drvr);
  if (!buffer)
    return -EINVAL;
  *buffer = kmm_malloc(buflen ? buflen : 1);
  return *buffer ? 0 : -ENOMEM;
}

static int esp32s31_hcd_iofree(struct usbhost_driver_s *drvr,
                               uint8_t *buffer)
{
  return esp32s31_hcd_free(drvr, buffer);
}

static int esp32s31_hcd_control(struct usbhost_driver_s *drvr, usbhost_ep_t ep,
                                const struct usb_ctrlreq_s *req, uint8_t *buffer)
{
  struct esp32s31_usbhost_request_s request;
  struct esp32s31_usbhost_chan_s *chan;
  struct s31_usb_pio_s pio;
  int ret;

  if (!req || ((req->len[0] || req->len[1]) && !buffer))
    return -EINVAL;
  ret = esp32s31_hcd_begin(drvr, ep, true, &request);
  if (ret < 0)
    return ret;
  chan = request.chan;
  esp32s31_hcd_pio_setup(&pio, &request);
  ret = s31_usb_control(s31_usb_pio_packet, &pio, chan->maxpacket,
                        (const uint8_t *)req, buffer);
  /* Successful CLEAR_FEATURE(ENDPOINT_HALT) resets the data toggle. */
  if (ret == 0 && req->type == USB_REQ_RECIPIENT_ENDPOINT &&
      req->req == USB_REQ_CLEARFEATURE && req->value[0] == 0 &&
      req->value[1] == 0 && req->len[0] == 0 && req->len[1] == 0)
    {
      unsigned int i;
      for (i = 1; i < ESP32S31_USBHOST_HCD_MAX_CHANNELS; i++)
        {
          struct esp32s31_usbhost_chan_s *target =
            &g_esp32s31_usbhost_hcd.chan[i];
          if (target->allocated && target->funcaddr == chan->funcaddr &&
              target->epnum == (req->index[0] & 15) &&
              target->in == ((req->index[0] & 128) != 0))
            target->pid = OTG_PID_DATA0;
        }
    }
  ret = esp32s31_hcd_finish(&request, ret);
  nxmutex_unlock(&g_pio_lock);
  return ret;
}

static int esp32s31_hcd_ctrlin(struct usbhost_driver_s *drvr,
                               usbhost_ep_t ep0,
                               const struct usb_ctrlreq_s *req,
                               uint8_t *buffer)
{
  return esp32s31_hcd_control(drvr, ep0, req, buffer);
}

static int esp32s31_hcd_ctrlout(struct usbhost_driver_s *drvr,
                                usbhost_ep_t ep0,
                                const struct usb_ctrlreq_s *req,
                                const uint8_t *buffer)
{
  if (!req || (((const uint8_t *)req)[0] & 0x80))
    return -EINVAL;
  return esp32s31_hcd_control(drvr, ep0, req, (uint8_t *)buffer);
}

static ssize_t esp32s31_hcd_transfer(struct usbhost_driver_s *drvr,
                                     usbhost_ep_t ep, uint8_t *buffer,
                                     size_t buflen)
{
  struct esp32s31_usbhost_request_s request;
  struct esp32s31_usbhost_chan_s *chan;
  struct s31_usb_pio_s pio;
  ssize_t ret;

  if (buflen && !buffer)
    return -EINVAL;
  ret = esp32s31_hcd_begin(drvr, ep, false, &request);
  if (ret < 0)
    return ret;
  chan = request.chan;
  esp32s31_hcd_pio_setup(&pio, &request);
  if (pio.periodic)
    {
      /* Interrupt readers (e.g. HID mouse) expect a report or EAGAIN.
       * Return one packet promptly, even when the allocated buffer is
       * larger than the endpoint packet. Callers may handle short writes.
       */
      if (!esp32s31_hcd_periodic_due(chan))
        ret = -EAGAIN;
      else
        {
          ret = s31_usb_pio_packet(&pio, chan->in, chan->pid, buffer,
                                   buflen < chan->maxpacket ?
                                   buflen : chan->maxpacket);
          esp32s31_hcd_periodic_record(chan, &pio);
          if (ret >= 0)
            chan->pid ^= 2;
          else if (ret == -EXDEV)
            ret = -EAGAIN;
        }
    }
  else
    ret = s31_usb_data(s31_usb_pio_packet, &pio, chan->in, chan->maxpacket,
                       &chan->pid, buffer, buflen);
  if (ret < 0 && ret != -EAGAIN)
    syslog(LOG_ERR, "USB transfer error=%d in=%u len=%u ch=%u phase=%u hcint=%08lx hctsiz=%08lx ping=%u budget=%u\n",
           (int)ret, chan->in, (unsigned int)buflen, chan->channel,
           chan->transfer.phase,
           (unsigned long)getreg32(ESP32S31_OTG_HCINT(chan->channel)),
           (unsigned long)getreg32(ESP32S31_OTG_HCTSIZ(chan->channel)),
           chan->need_ping, pio.budget);
  ret = esp32s31_hcd_finish(&request, ret);
  nxmutex_unlock(&g_pio_lock);
  return ret;
}

#ifdef CONFIG_USBHOST_ASYNCH
/* Queue helpers require g_request_lock. A wake is coalesced until the worker
 * consumes it, so repeated enqueue/cancel cannot grow the semaphore count.
 */

static bool esp32s31_hcd_async_wake(void)
{
  if (g_async_head && !g_async_wakeup)
    {
      g_async_wakeup = true;
      return true;
    }
  return false;
}

static void esp32s31_hcd_async_append(struct esp32s31_usbhost_async_s *req)
{
  req->next = NULL;
  if (g_async_tail)
    g_async_tail->next = req;
  else
    g_async_head = req;
  g_async_tail = req;
}

static void esp32s31_hcd_async_remove(struct esp32s31_usbhost_async_s *req)
{
  struct esp32s31_usbhost_async_s *prev = NULL;
  struct esp32s31_usbhost_async_s *item = g_async_head;
  while (item && item != req)
    {
      prev = item;
      item = item->next;
    }
  DEBUGASSERT(item == req);
  if (prev)
    prev->next = req->next;
  else
    g_async_head = req->next;
  if (g_async_tail == req)
    g_async_tail = prev;
}

static bool esp32s31_hcd_async_finish(struct esp32s31_usbhost_async_s *req,
                                     ssize_t result)
{
  struct esp32s31_usbhost_chan_s *chan = req->request.chan;
  usbhost_asynch_t callback = NULL;
  void *arg = NULL;
  irqstate_t critical;
  irqstate_t flags;
  bool retry;
  bool wake;

  /* On this !SMP port the callback claim and invocation are one critical
   * operation, as in interrupt-driven HCDs. CANCEL cannot return between
   * these two steps and let the class free the callback argument first.
   * The callback holds neither the request spinlock nor the FIFO mutex.
   */
  critical = enter_critical_section();
  flags = spin_lock_irqsave(&g_request_lock);
  DEBUGASSERT(chan->async_active == req);
  chan->async_active = NULL;
  req->running = false;
  if (req->request.cancelled)
    result = -ESHUTDOWN;
  retry = result == -EAGAIN;
  if (retry)
    esp32s31_hcd_async_append(req);
  else
    {
      if (chan->async == req)
        chan->async = NULL;
      callback = req->callback;
      arg = req->arg;
      req->callback = NULL;
    }
  wake = esp32s31_hcd_async_wake();
  spin_unlock_irqrestore(&g_request_lock, flags);
  if (callback)
    callback(arg, result);
  leave_critical_section(critical);
  if (!retry)
    kmm_free(req);
  /* A queued request can be cancelled and freed already. Do not touch it. */
  if (wake)
    nxsem_post(&g_async_sem);
  return retry;
}

static bool esp32s31_hcd_async_step(struct esp32s31_usbhost_async_s *req)
{
  struct esp32s31_usbhost_chan_s *chan = req->request.chan;
  struct s31_usb_pio_s pio;
  irqstate_t flags;
  size_t count = 0;
  ssize_t ret;

  ret = nxmutex_lock(&g_pio_lock);
  if (ret == 0)
    {
      flags = spin_lock_irqsave(&g_request_lock);
      if (req->request.cancelled)
        ret = -ESHUTDOWN;
      else if (g_request || g_resetting)
        ret = -EAGAIN; /* Give a waiting synchronous control request priority. */
      else if (chan->xfrtype == USB_EP_ATTR_XFER_INT &&
               !esp32s31_hcd_periodic_due(chan))
        ret = -EAGAIN;
      else
        {
          count = req->length - req->offset;
          if (count > chan->maxpacket)
            count = chan->maxpacket;
          if (!chan->in && count)
            memcpy(req->packet, req->buffer + req->offset, count);
        }
      spin_unlock_irqrestore(&g_request_lock, flags);
      if (ret == 0)
        ret = esp32s31_hcd_fifo_ready();
      if (ret == 0)
        {
          esp32s31_hcd_pio_setup(&pio, &req->request);
          if (!pio.periodic)
            pio.deadline_ms = req->deadline;
          pio.yield_nak = true;
          /* Cancellation retires the client immediately. Let an already
           * launched packet finish in private storage so an accepted OUT
           * cannot lose its data toggle. NAK yields; the original deadline
           * still bounds a controller that never completes the packet.
           */
          pio.finish_active = true;
          ret = s31_usb_pio_packet(&pio, chan->in, chan->pid,
                                   req->packet, count);
          esp32s31_hcd_periodic_record(chan, &pio);
          if (pio.periodic && ret == -EXDEV)
            ret = -EAGAIN;
          if (ret >= 0)
            {
              /* Confirmed packets advance the toggle even if cancellation
               * won before callback retirement. Never copy after cancel.
               */
              flags = spin_lock_irqsave(&g_request_lock);
              chan->pid ^= 2;
              if (!req->request.cancelled)
                {
                  if (chan->in && ret)
                    memcpy(req->buffer + req->offset, req->packet, ret);
                  req->offset += ret;
                }
              ret = (pio.periodic && chan->in) ||
                    (size_t)ret < count || req->offset == req->length ?
                    (ssize_t)req->offset : -EAGAIN;
              spin_unlock_irqrestore(&g_request_lock, flags);
            }
        }
      nxmutex_unlock(&g_pio_lock);
    }
  /* Idle interrupt IN can NAK indefinitely, e.g. an untouched keyboard.
   * Only an armed controller packet has a finite deadline on that path.
   */
  if (ret == -EAGAIN && chan->xfrtype != USB_EP_ATTR_XFER_INT &&
      esp32s31_hcd_now_ms() >= req->deadline)
    ret = -ETIMEDOUT;
  return esp32s31_hcd_async_finish(req, ret);
}

static int esp32s31_hcd_async_worker(int argc, char **argv)
{
  struct esp32s31_usbhost_async_s *req;
  irqstate_t flags;
  UNUSED(argc);
  UNUSED(argv);
  for (;;)
    {
      nxsem_wait_uninterruptible(&g_async_sem);
      flags = spin_lock_irqsave(&g_request_lock);
      g_async_wakeup = false;
      req = g_async_head;
      if (req)
        {
          esp32s31_hcd_async_remove(req);
          req->running = true;
          req->request.chan->async_active = req;
        }
      spin_unlock_irqrestore(&g_request_lock, flags);
      if (req && esp32s31_hcd_async_step(req))
        nxsig_usleep(1000); /* NAK and long requests must yield the CPU too. */
    }
  return 0;
}

static int esp32s31_hcd_async_start(void)
{
  int ret;
  if (g_async_started)
    return 0;
  ret = kthread_create("s31-usb-async", 100, 4096,
                       esp32s31_hcd_async_worker, NULL);
  if (ret < 0)
    return ret;
  g_async_started = true;
  return 0;
}

static int esp32s31_hcd_asynch(struct usbhost_driver_s *drvr,
                               usbhost_ep_t ep, uint8_t *buffer,
                               size_t buflen, usbhost_asynch_t callback,
                               void *arg)
{
  struct esp32s31_usbhost_async_s *req;
  struct esp32s31_usbhost_chan_s *chan;
  irqstate_t flags;
  bool wake = false;
  int ret = 0;

  if (drvr != &g_esp32s31_usbhost_hcd.drvr || !callback ||
      (buflen && !buffer) || buflen > SSIZE_MAX)
    return -EINVAL;
  if (!g_async_started)
    return -ENODEV;
  req = kmm_zalloc(sizeof(*req));
  if (!req)
    return -ENOMEM;
  req->buffer = buffer;
  req->length = buflen;
  req->callback = callback;
  req->arg = arg;
  req->deadline = esp32s31_hcd_now_ms() + 5000;
  flags = spin_lock_irqsave(&g_request_lock);
  chan = esp32s31_hcd_endpoint(ep);
  if (!chan)
    ret = -EINVAL;
  else if (chan->xfrtype != USB_EP_ATTR_XFER_BULK &&
           chan->xfrtype != USB_EP_ATTR_XFER_INT)
    ret = -ENOTSUP;
  else if (g_resetting || chan->async ||
           (g_request && g_request->chan == chan))
    ret = -EBUSY;
  else
    {
      req->request.chan = chan;
      chan->async = req;
      esp32s31_hcd_async_append(req);
      wake = esp32s31_hcd_async_wake();
    }
  spin_unlock_irqrestore(&g_request_lock, flags);
  if (ret < 0)
    kmm_free(req);
  if (wake)
    nxsem_post(&g_async_sem);
  return ret;
}
#endif

static int esp32s31_hcd_cancel(struct usbhost_driver_s *drvr,
                               usbhost_ep_t ep)
{
  struct esp32s31_usbhost_chan_s *chan;
  irqstate_t flags;
  int ret = 0;
#ifdef CONFIG_USBHOST_ASYNCH
  struct esp32s31_usbhost_async_s *req = NULL;
  usbhost_asynch_t callback = NULL;
  void *arg = NULL;
  bool release = false;
  irqstate_t critical;
#endif

  if (drvr != &g_esp32s31_usbhost_hcd.drvr)
    return -EINVAL;
  if (ep == NULL)
    ep = &g_esp32s31_usbhost_hcd.chan[0];
#ifdef CONFIG_USBHOST_ASYNCH
  critical = enter_critical_section();
#endif
  flags = spin_lock_irqsave(&g_request_lock);
  chan = esp32s31_hcd_endpoint(ep);
  if (!chan)
    ret = -EINVAL;
  else if (g_request && g_request->chan == chan)
    g_request->cancelled = true;
#ifdef CONFIG_USBHOST_ASYNCH
  else if (chan->async)
    {
      req = chan->async;
      req->request.cancelled = true;
      chan->async = NULL;
      callback = req->callback;
      arg = req->arg;
      req->callback = NULL;
      req->buffer = NULL;
      release = !req->running;
      if (release)
        esp32s31_hcd_async_remove(req);
    }
#endif
  spin_unlock_irqrestore(&g_request_lock, flags);
#ifdef CONFIG_USBHOST_ASYNCH
  if (callback)
    callback(arg, -ESHUTDOWN);
  leave_critical_section(critical);
  if (release)
    kmm_free(req);
#endif
  return ret;
}

#ifdef CONFIG_USBHOST_HUB
static int esp32s31_hcd_connect(struct usbhost_driver_s *drvr,
                                struct usbhost_hubport_s *hport,
                                bool connected)
{
  UNUSED(drvr);
  UNUSED(hport);
  UNUSED(connected);
  return -ENOSYS;
}
#endif

static void esp32s31_hcd_disconnect(struct usbhost_driver_s *drvr,
                                    struct usbhost_hubport_s *hport)
{
  if (drvr == &g_esp32s31_usbhost_hcd.drvr &&
      hport == &g_esp32s31_usbhost_hcd.rhport.hport)
    hport->devclass = NULL;
}

struct usbhost_connection_s *esp32s31_usbhost_hcd_connection_skeleton(void)
{
  struct usbhost_connection_s *conn = &g_esp32s31_usbhost_hcd.conn;

  conn->wait = esp32s31_hcd_wait;
  conn->enumerate = esp32s31_hcd_enumerate;
  return conn;
}

struct usbhost_driver_s *esp32s31_usbhost_hcd_skeleton(void)
{
  struct usbhost_driver_s *drvr = &g_esp32s31_usbhost_hcd.drvr;

  drvr->ep0configure = esp32s31_hcd_ep0configure;
  drvr->epalloc = esp32s31_hcd_epalloc;
  drvr->epfree = esp32s31_hcd_epfree;
  drvr->alloc = esp32s31_hcd_alloc;
  drvr->free = esp32s31_hcd_free;
  drvr->ioalloc = esp32s31_hcd_ioalloc;
  drvr->iofree = esp32s31_hcd_iofree;
  drvr->ctrlin = esp32s31_hcd_ctrlin;
  drvr->ctrlout = esp32s31_hcd_ctrlout;
  drvr->transfer = esp32s31_hcd_transfer;
#ifdef CONFIG_USBHOST_ASYNCH
  drvr->asynch = esp32s31_hcd_asynch;
#endif
  drvr->cancel = esp32s31_hcd_cancel;
#ifdef CONFIG_USBHOST_HUB
  drvr->connect = esp32s31_hcd_connect;
#endif
  drvr->disconnect = esp32s31_hcd_disconnect;
  return drvr;
}

static int esp32s31_usbhost_worker(int argc, char **argv)
{
  struct usbhost_hubport_s *hport;
  struct usbhost_connection_s *conn = esp32s31_usbhost_hcd_connection_skeleton();
  int ret;
  UNUSED(argc);
  UNUSED(argv);
  for (;;)
    {
      ret = conn->wait(conn, &hport);
      if (ret == 0 && hport->connected)
        {
          ret = conn->enumerate(conn, hport);
          syslog(ret < 0 ? LOG_ERR : LOG_INFO,
                 "USB host enumeration returned %d speed=%u\n",
                 ret, (unsigned int)hport->speed);
        }
      else if (ret < 0)
        nxsig_usleep(100000);
    }
  return 0;
}

int esp32s31_usbhost_hcd_start(void)
{
  struct esp32s31_usbhost_hcd_s *priv = &g_esp32s31_usbhost_hcd;
  static bool started;
  int ret;
  if (started)
    return -EALREADY;
  priv->rhport.hport.drvr = esp32s31_usbhost_hcd_skeleton();
  priv->rhport.hport.ep0 = &priv->chan[0];
  priv->rhport.pdevgen = &priv->devgen;
  ret = usbhost_devaddr_initialize(&priv->devgen);
  if (ret < 0)
    return ret;
#ifdef CONFIG_USBHOST_MSC
  ret = usbhost_msc_initialize();
  if (ret < 0)
    return ret;
#endif
#ifdef CONFIG_USBHOST_HIDKBD
  ret = usbhost_kbdinit();
  if (ret < 0)
    return ret;
#endif
#ifdef CONFIG_USBHOST_HIDMOUSE
  ret = usbhost_mouse_init();
  if (ret < 0)
    return ret;
#endif
#ifdef CONFIG_USBHOST_ASYNCH
  ret = esp32s31_hcd_async_start();
  if (ret < 0)
    return ret;
#endif
  ret = kthread_create("s31-usbhost", 100, 4096,
                       esp32s31_usbhost_worker, NULL);
  if (ret < 0)
    return ret;
  started = true;
  return 0;
}

int esp32s31_usbhost_hcd_skeleton_contract(void)
{
  struct esp32s31_usbhost_chan_s sample = {0};
  struct esp32s31_usbhost_chan_s *chan = &sample;
  struct usbhost_driver_s *drvr;

  chan->channel = 0;
  chan->funcaddr = 0;
  chan->epnum = 0;
  chan->xfrtype = USB_EP_ATTR_XFER_CONTROL;
  chan->maxpacket = 64;
  chan->in = false;
  chan->state = ESP32S31_USBHOST_CH_RESET;
  esp32s31_hcd_channel_prepare(chan);
  if (chan->state != ESP32S31_USBHOST_CH_READY ||
      (chan->hcchar & OTG_HCCHAR_MPSIZ_MASK) != 64 ||
      (chan->hctsiz & OTG_HCTSIZ_PKTCNT_MASK) !=
        (1u << OTG_HCTSIZ_PKTCNT_SHIFT))
    {
      return -EINVAL;
    }

  drvr = esp32s31_usbhost_hcd_skeleton();
  if (drvr->ep0configure == NULL ||
      drvr->epalloc == NULL ||
      drvr->epfree == NULL ||
      drvr->alloc == NULL ||
      drvr->free == NULL ||
      drvr->ioalloc == NULL ||
      drvr->iofree == NULL ||
      drvr->ctrlin == NULL ||
      drvr->ctrlout == NULL ||
      drvr->transfer == NULL ||
      drvr->cancel == NULL ||
      drvr->disconnect == NULL)
    {
      return -EINVAL;
    }

  return esp32s31_usbhost_hcd_connection_skeleton()->wait != NULL &&
         g_esp32s31_usbhost_hcd.conn.enumerate != NULL ? 0 : -EINVAL;
}

#endif /* CONFIG_ESP32S31_USBHOST_HCD_SKELETON && CONFIG_ESP32S31_USBHOST */
