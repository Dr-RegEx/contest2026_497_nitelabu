/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_usbhost_hcd_skeleton.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <nuttx/arch.h>
#include <nuttx/signal.h>
#include <nuttx/kthread.h>
#include <syslog.h>
#include <nuttx/usb/usbhost_devaddr.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <nuttx/mutex.h>
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

/* Experimental HCD: serialized polling PIO implements control and bulk
 * transfers and a polling root port with the common NuttX enumerator. IRQ
 * routing and periodic/asynchronous scheduling remain absent.
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

static bool esp32s31_hcd_packet_valid(uint8_t speed, uint8_t type, uint16_t mps)
{
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

static void esp32s31_hcd_pio_setup(struct s31_usb_pio_s *pio,
                                  struct esp32s31_usbhost_chan_s *chan)
{
  memset(pio, 0, sizeof(*pio));
  pio->io = g_channel_io;
  pio->xfer = &chan->transfer;
  pio->delay = esp32s31_hcd_delay;
  pio->maxpacket = chan->maxpacket;
  pio->highspeed = chan->speed == USB_SPEED_HIGH;
  pio->need_ping = &chan->need_ping;
  pio->channel = chan->channel;
  pio->budget = 10000; /* One second for the entire synchronous request. */
  pio->hcchar = esp32s31_hcd_hcchar(chan) | (1u << OTG_HCCHAR_MCNT_SHIFT);
  if (chan->speed == USB_SPEED_LOW)
    pio->hcchar |= OTG_HCCHAR_LSDEV;
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
          if (!connected && port->devclass && !priv->detached)
            {
              priv->detached = true;
              CLASS_DISCONNECTED(port->devclass);
            }
          *hport = port;
          return 0;
        }
      if (nxsig_usleep(10000) < 0)
        return -EINTR;
    }
}

static int esp32s31_hcd_enumerate(struct usbhost_connection_s *conn,
                                  struct usbhost_hubport_s *hport)
{
  struct esp32s31_usbhost_hcd_s *priv = &g_esp32s31_usbhost_hcd;
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
  for (i = 0; i < ESP32S31_USBHOST_HCD_MAX_CHANNELS; i++)
    if ((i && priv->chan[i].allocated) ||
        priv->chan[i].transfer.phase == S31_HC_STOPPING ||
        priv->chan[i].transfer.phase == S31_HC_ACTIVE)
      {
        nxmutex_unlock(&g_pio_lock);
        return -EBUSY;
      }
  speed = s31_port_reset(&g_channel_io, esp32s31_port_delay);
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
  int ret = nxmutex_lock(&g_pio_lock);
  if (ret < 0)
    return ret;
  chan = ep0 == NULL ? &g_esp32s31_usbhost_hcd.chan[0] :
                       esp32s31_hcd_endpoint(ep0);
  if (!chan || drvr != &g_esp32s31_usbhost_hcd.drvr || funcaddr > 127 ||
      !esp32s31_hcd_packet_valid(speed, USB_EP_ATTR_XFER_CONTROL, maxpacketsize))
    ret = -EINVAL;
  else if (chan->transfer.phase == S31_HC_ACTIVE ||
           chan->transfer.phase == S31_HC_STOPPING)
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
  nxmutex_unlock(&g_pio_lock);
  return ret;
}

static int esp32s31_hcd_epalloc(struct usbhost_driver_s *drvr,
                                const struct usbhost_epdesc_s *epdesc,
                                usbhost_ep_t *ep)
{
  unsigned int i;
  int ret;
  if (ep == NULL)
    return -EINVAL;
  *ep = NULL;
  if (drvr != &g_esp32s31_usbhost_hcd.drvr || !epdesc || !epdesc->hport ||
      epdesc->hport->funcaddr > 127 || (epdesc->addr & 0x70) ||
      (epdesc->xfrtype == USB_EP_ATTR_XFER_BULK &&
       (epdesc->addr & 0x0f) == 0))
    return -EINVAL;
#ifdef CONFIG_USBHOST_HUB
  if (epdesc->hport->parent != NULL)
    return -ENOTSUP; /* Split transactions/hub routing are not implemented. */
#endif
  if (!esp32s31_hcd_packet_valid(epdesc->hport->speed, epdesc->xfrtype,
                                epdesc->mxpacketsize))
    return -ENOTSUP;
  ret = nxmutex_lock(&g_pio_lock);
  if (ret < 0)
    return ret;
  ret = -ENOMEM;
  for (i = 1; i < ESP32S31_USBHOST_HCD_MAX_CHANNELS; i++)
    if (!g_esp32s31_usbhost_hcd.chan[i].allocated)
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
        *ep = chan;
        ret = 0;
        break;
      }
  nxmutex_unlock(&g_pio_lock);
  return ret;
}

static int esp32s31_hcd_epfree(struct usbhost_driver_s *drvr,
                               usbhost_ep_t ep)
{
  struct esp32s31_usbhost_chan_s *chan;
  int ret = nxmutex_lock(&g_pio_lock);
  if (ret < 0)
    return ret;
  chan = esp32s31_hcd_endpoint(ep);
  if (!chan || drvr != &g_esp32s31_usbhost_hcd.drvr)
    ret = -EINVAL;
  else if (chan->transfer.phase == S31_HC_ACTIVE ||
           chan->transfer.phase == S31_HC_STOPPING)
    ret = -EBUSY;
  else
    {
      chan->allocated = false;
      ret = 0;
    }
  nxmutex_unlock(&g_pio_lock);
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
  struct esp32s31_usbhost_chan_s *chan;
  struct s31_usb_pio_s pio;
  int ret = nxmutex_lock(&g_pio_lock);
  if (ret < 0)
    return ret;
  chan = esp32s31_hcd_endpoint(ep == NULL ?
           &g_esp32s31_usbhost_hcd.chan[0] : ep);
  if (!chan || !req || chan->xfrtype != USB_EP_ATTR_XFER_CONTROL ||
      drvr != &g_esp32s31_usbhost_hcd.drvr)
    ret = -EINVAL;
  else
    {
      esp32s31_hcd_pio_setup(&pio, chan);
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
    }
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
  struct esp32s31_usbhost_chan_s *chan;
  struct s31_usb_pio_s pio;
  ssize_t ret = nxmutex_lock(&g_pio_lock);
  if (ret < 0)
    return ret;
  chan = esp32s31_hcd_endpoint(ep);
  if (!chan || drvr != &g_esp32s31_usbhost_hcd.drvr)
    ret = -EINVAL;
  else if (chan->xfrtype != USB_EP_ATTR_XFER_BULK)
    ret = -ENOTSUP;
  else
    {
      esp32s31_hcd_pio_setup(&pio, chan);
      ret = s31_usb_data(s31_usb_pio_packet, &pio, chan->in, chan->maxpacket,
                         &chan->pid, buffer, buflen);
    }
  nxmutex_unlock(&g_pio_lock);
  return ret;
}

#ifdef CONFIG_USBHOST_ASYNCH
static int esp32s31_hcd_asynch(struct usbhost_driver_s *drvr,
                               usbhost_ep_t ep, uint8_t *buffer,
                               size_t buflen, usbhost_asynch_t callback,
                               void *arg)
{
  UNUSED(drvr);
  UNUSED(ep);
  UNUSED(buffer);
  UNUSED(buflen);
  UNUSED(callback);
  UNUSED(arg);
  return -ENOSYS;
}
#endif

static int esp32s31_hcd_cancel(struct usbhost_driver_s *drvr,
                               usbhost_ep_t ep)
{
  UNUSED(drvr);
  UNUSED(ep);
  return -ENOSYS;
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
                 "USB host enumeration returned %d\n", ret);
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
  ret = kthread_create("s31-usbhost", 100, 4096,
                       esp32s31_usbhost_worker, NULL);
  if (ret < 0)
    return ret;
  started = true;
  return 0;
}

int esp32s31_usbhost_hcd_skeleton_contract(void)
{
  struct esp32s31_usbhost_chan_s *chan = &g_esp32s31_usbhost_hcd.chan[0];
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
