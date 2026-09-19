/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __ARCH_RISCV_SRC_ESP32S31_USBHOST_PIO_H
#define __ARCH_RISCV_SRC_ESP32S31_USBHOST_PIO_H
#include <sys/types.h>
#include "esp32s31_usbhost_channel.h"

/* One serialized non-periodic packet. Only the FIFO is accessed by CPU:
 * no user address is handed to the DMA engine. The budget is shared by all
 * packets/stages/retries of a request, in 100-us polling quanta.
 */
struct s31_usb_pio_s
{
  struct s31_hc_io_s io;
  struct s31_hc_transfer_s *xfer;
  void (*delay)(void *arg, unsigned int us);
  uint32_t hcchar;
  unsigned int channel;
  unsigned int budget;
  uint16_t maxpacket;
  bool highspeed;
  bool *need_ping;
};

static inline bool s31_pio_tick(struct s31_usb_pio_s *pio)
{
  if (pio->budget == 0)
    return false;
  pio->budget--;
  pio->delay(pio->io.arg, 100);
  return true;
}

static inline int s31_pio_flush(struct s31_usb_pio_s *pio)
{
  const struct s31_hc_io_s *io = &pio->io;
  const uint32_t bits = OTG_GRSTCTL_RXFFLSH | OTG_GRSTCTL_TXFFLSH;
  io->write(io->arg, ESP32S31_OTG_GRSTCTL_OFFSET, bits);
  while (io->read(io->arg, ESP32S31_OTG_GRSTCTL_OFFSET) & bits)
    if (!s31_pio_tick(pio))
      return -ETIMEDOUT;
  return 0;
}

static inline ssize_t s31_usb_pio_packet(void *arg, bool in, uint8_t pid,
                                       uint8_t *buffer, size_t length)
{
  struct s31_usb_pio_s *pio = arg;
  const struct s31_hc_io_s *io = &pio->io;
  uint32_t hcchar = pio->hcchar;
  uint32_t status;
  uint32_t word;
  size_t received;
  size_t bytes;
  size_t i;
  unsigned int j;
  int rxerror;
  int abortret;
  int ret;
  bool ping;
  bool hsout = !in && pio->highspeed && pid != 3;

  if (length > pio->maxpacket || !pio->maxpacket ||
      pio->maxpacket > 512 || pid > 3 || (length && !buffer))
    return -EINVAL;
  if ((io->read(io->arg, ESP32S31_OTG_GINTSTS_OFFSET) & OTG_GINTSTS_CMOD) == 0 ||
      (io->read(io->arg, ESP32S31_OTG_GAHBCFG_OFFSET) & (1u << 5)))
    return -ENOTSUP; /* Must be host, and DMA must be disabled. */
  hcchar &= ~((1u << 31) | (1u << 30) | OTG_HCCHAR_EPDIR_IN);
  if (in)
    hcchar |= OTG_HCCHAR_EPDIR_IN;

  if (pid == 3 && pio->need_ping)
    *pio->need_ping = false; /* SETUP starts a new control transaction. */

  for (;;)
    {
      ping = hsout && pio->need_ping && *pio->need_ping;
      if (!pio->budget)
        return -ETIMEDOUT;
      if ((io->read(io->arg, ESP32S31_OTG_HPRT_OFFSET) &
           (OTG_HPRT_PCSTS | OTG_HPRT_PENA)) !=
          (OTG_HPRT_PCSTS | OTG_HPRT_PENA))
        return -ENODEV;
      if (pio->xfer->phase == S31_HC_ACTIVE ||
          pio->xfer->phase == S31_HC_STOPPING)
        return -EBUSY;
      if (io->read(io->arg, ESP32S31_OTG_HCCHAR_OFFSET(pio->channel)) & (1u << 31))
        return -EBUSY;
      ret = s31_pio_flush(pio);
      if (ret < 0)
        return ret;
      do
        {
          status = io->read(io->arg, ESP32S31_OTG_HNPTXSTS_OFFSET);
          if ((status & OTG_HNPTXSTS_NPTQXSAV_MASK) != 0 &&
              (in || ping || (status & 0xffff) >= (length + 3) / 4))
            break;
          if (!s31_pio_tick(pio))
            return -ETIMEDOUT;
        }
      while (true);
      io->write(io->arg, ESP32S31_OTG_HCCHAR_OFFSET(pio->channel), hcchar);
      io->write(io->arg, ESP32S31_OTG_HCTSIZ_OFFSET(pio->channel),
                (uint32_t)(ping ? 0 : (in ? pio->maxpacket : length)) |
                (ping ? (1u << 31) : 0) |
                (1u << OTG_HCTSIZ_PKTCNT_SHIFT) |
                ((uint32_t)pid << OTG_HCTSIZ_DPID_SHIFT));
      ret = s31_hc_arm(pio->xfer, io, pio->channel,
                       ping ? 0 : (in ? pio->maxpacket : length));
      if (ret < 0)
        return ret;
      pio->xfer->hsout = hsout;
      pio->xfer->ping = ping;
      received = 0;
      rxerror = 0;
      abortret = -ETIMEDOUT;
      io->write(io->arg, ESP32S31_OTG_HCCHAR_OFFSET(pio->channel),
                hcchar | (1u << 31));
      if (!in && !ping)
        for (i = 0; i < length; i += 4)
          {
            word = 0;
            for (j = 0; j < 4 && i + j < length; j++)
              word |= (uint32_t)buffer[i + j] << (8 * j);
            io->write(io->arg, ESP32S31_OTG_DFIFO_HCH_OFFSET(pio->channel), word);
          }
      for (;;)
        {
          /* Drain RX status before completing XFRC; always drain excess
           * bytes without writing beyond the caller's buffer.
           */
          if (io->read(io->arg, ESP32S31_OTG_GINTSTS_OFFSET) & OTG_GINT_RXFLVL)
            {
              status = io->read(io->arg, ESP32S31_OTG_GRXSTSP_OFFSET);
              bytes = (status & OTG_GRXSTSH_BCNT_MASK) >> OTG_GRXSTSH_BCNT_SHIFT;
              if ((status & OTG_GRXSTSH_PKTSTS_MASK) == OTG_GRXSTSH_PKTSTS_INRECVD)
                {
                  if (!in || (status & OTG_GRXSTSH_CHNUM_MASK) != pio->channel ||
                      received + bytes > length)
                    rxerror = -EOVERFLOW;
                  for (i = 0; i < bytes; i += 4)
                    {
                      word = io->read(io->arg, ESP32S31_OTG_DFIFO_HCH_OFFSET(0));
                      for (j = 0; j < 4 && i + j < bytes; j++)
                        if (!rxerror && received + i + j < length)
                          buffer[received + i + j] = word >> (8 * j);
                    }
                  received += bytes;
                }
              /* There may be a separate transfer-complete RX status. */
              if (!s31_pio_tick(pio))
                break;
              continue;
            }
          if (!(io->read(io->arg, ESP32S31_OTG_HPRT_OFFSET) & OTG_HPRT_PCSTS))
            {
              abortret = -ENODEV;
              break;
            }
          if (s31_hc_interrupt(pio->xfer, io, pio->channel))
            break;
          if (!s31_pio_tick(pio))
            break;
        }
      if (pio->xfer->phase != S31_HC_DONE)
        {
          if (pio->xfer->phase == S31_HC_ACTIVE)
            s31_hc_stop(pio->xfer, io, pio->channel,
                        abortret == -ENODEV ? -ECANCELED : -ETIMEDOUT);
          /* A bounded extra halt grace period never makes an unhalted
           * channel reusable. Retain STOPPING if hardware never replies.
           */
          for (i = 0; i < 100; i++)
            {
              if (s31_hc_interrupt(pio->xfer, io, pio->channel))
                break;
              pio->delay(io->arg, 100);
            }
          return abortret;
        }
      if (pio->xfer->result == -EAGAIN)
        {
          if (hsout && pio->need_ping)
            *pio->need_ping = true;
          if (!s31_pio_tick(pio))
            return -ETIMEDOUT;
          continue;
        }
      if (pio->xfer->result < 0)
        return pio->xfer->result;
      if (ping)
        {
          *pio->need_ping = false;
          continue; /* ACK only permits data; it never completes it. */
        }
      if (hsout && pio->need_ping)
        *pio->need_ping = pio->xfer->nyet;
      if (rxerror)
        return rxerror;
      /* In slave mode OUT HCTSIZ may retain its programmed length.
       * Successful XFRC followed by CHH confirms the entire OUT packet;
       * only IN uses the residual count to validate received FIFO bytes.
       */
      if (in && received != pio->xfer->transferred)
        return -EIO;
      return in ? (ssize_t)received : (ssize_t)length;
    }
}
#endif
