/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __ARCH_RISCV_SRC_ESP32S31_USBHOST_CHANNEL_H
#define __ARCH_RISCV_SRC_ESP32S31_USBHOST_CHANNEL_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include "hardware/esp32s31_otg.h"

/* S31 DWC2 has AHBERR at bit 2 (unlike the old STM32 FS register comment).
 * Descriptor-DMA-only interrupts are deliberately not enabled here.
 */

#define S31_HC_AHBERR (1u << 2)
#define S31_HC_IRQ_MASK 0x7ffu

enum s31_hc_phase_e
{
  S31_HC_IDLE = 0,
  S31_HC_ACTIVE,
  S31_HC_STOPPING,
  S31_HC_DONE
};

struct s31_hc_transfer_s
{
  enum s31_hc_phase_e phase;
  uint32_t programmed;
  uint32_t transferred;
  int result;
  bool hsout;
  bool ping;
  bool nyet;
};

/* Register access is supplied by the HCD. Host tests replace only these
 * MMIO operations. The caller serializes all calls with its IRQ-safe lock.
 */

struct s31_hc_io_s
{
  void *arg;
  uint32_t (*read)(void *arg, uint32_t offset);
  void (*write)(void *arg, uint32_t offset, uint32_t value);
};

static inline int s31_hc_arm(struct s31_hc_transfer_s *xfer,
                            const struct s31_hc_io_s *io,
                            unsigned int channel, uint32_t programmed)
{
  if (channel >= 16 || programmed > OTG_HCTSIZ_XFRSIZ_MASK)
    {
      return -EINVAL;
    }

  /* A software timeout alone cannot make a channel reusable. CHH must
   * arrive, and CHENA must be clear, before old status is drained.
   */

  if (xfer->phase == S31_HC_ACTIVE || xfer->phase == S31_HC_STOPPING ||
      (io->read(io->arg, ESP32S31_OTG_HCCHAR_OFFSET(channel)) &
       (1u << 31)) != 0)
    {
      return -EBUSY;
    }

  io->write(io->arg, ESP32S31_OTG_HCINTMSK_OFFSET(channel), 0);
  io->write(io->arg, ESP32S31_OTG_HCINT_OFFSET(channel), S31_HC_IRQ_MASK);
  xfer->hsout = false;
  xfer->ping = false;
  xfer->nyet = false;
  xfer->programmed = programmed;
  xfer->transferred = 0;
  xfer->result = -EINPROGRESS;
  xfer->phase = S31_HC_ACTIVE;
  io->write(io->arg, ESP32S31_OTG_HCINTMSK_OFFSET(channel), S31_HC_IRQ_MASK);
  return 0;
}

static inline void s31_hc_halt(const struct s31_hc_io_s *io,
                               unsigned int channel)
{
  uint32_t hcchar = io->read(io->arg, ESP32S31_OTG_HCCHAR_OFFSET(channel));

  /* DWC2 halt request: set both CHENA and CHDIS, preserving endpoint bits.
   * FIFO/request-queue scheduling remains the launcher's responsibility.
   */

  io->write(io->arg, ESP32S31_OTG_HCCHAR_OFFSET(channel),
            hcchar | (1u << 31) | (1u << 30));
}

static inline int s31_hc_stop(struct s31_hc_transfer_s *xfer,
                             const struct s31_hc_io_s *io,
                             unsigned int channel, int reason)
{
  if (channel >= 16 || (reason != -ETIMEDOUT && reason != -ECANCELED))
    {
      return -EINVAL;
    }

  if (xfer->phase != S31_HC_ACTIVE)
    {
      return -EALREADY;
    }

  xfer->result = reason;
  xfer->phase = S31_HC_STOPPING;
  s31_hc_halt(io, channel);
  return 0;
}

/* Return true once, and only after channel-halted. A NAK/NYET completion
 * returns EAGAIN to the scheduler: it is never a successful USB transfer.
 */

static inline bool s31_hc_interrupt(struct s31_hc_transfer_s *xfer,
                                   const struct s31_hc_io_s *io,
                                   unsigned int channel)
{
  uint32_t pending;
  uint32_t remaining;
  int result = -EINPROGRESS;

  if (channel >= 16)
    {
      return false;
    }

  pending = io->read(io->arg, ESP32S31_OTG_HCINT_OFFSET(channel)) &
            io->read(io->arg, ESP32S31_OTG_HCINTMSK_OFFSET(channel)) &
            S31_HC_IRQ_MASK;
  if (pending == 0)
    {
      return false;
    }

  io->write(io->arg, ESP32S31_OTG_HCINT_OFFSET(channel), pending);
  if (xfer->phase != S31_HC_ACTIVE && xfer->phase != S31_HC_STOPPING)
    {
      return false;
    }

  if ((pending & OTG_HCINT_NYET) && xfer->hsout)
    xfer->nyet = true;

  if (pending & OTG_HCINT_STALL)
    {
      result = -EPIPE;
    }
  else if (pending & S31_HC_AHBERR)
    {
      result = -EIO;
    }
  else if (pending & OTG_HCINT_BBERR)
    {
      result = -EOVERFLOW;
    }
  else if (pending & (OTG_HCINT_TXERR | OTG_HCINT_DTERR))
    {
      result = -EPROTO;
    }
  else if (pending & OTG_HCINT_FRMOR)
    {
      result = -EXDEV;
    }
  else if (pending & OTG_HCINT_XFRC)
    {
      remaining = io->read(io->arg, ESP32S31_OTG_HCTSIZ_OFFSET(channel)) &
                  OTG_HCTSIZ_XFRSIZ_MASK;
      result = remaining > xfer->programmed ? -EIO :
               (xfer->ping && !(pending & OTG_HCINT_ACK) ? -EPROTO : 0);
      if (result == 0)
        {
          xfer->transferred = xfer->programmed - remaining;
        }
    }
  else if (xfer->ping && (pending & OTG_HCINT_ACK))
    {
      result = 0; /* PING readiness, no data bytes transferred. */
    }
  else if (xfer->hsout && !xfer->ping && (pending & OTG_HCINT_NYET))
    {
      result = 0; /* One OUT packet accepted; next OUT must PING. */
      xfer->transferred = xfer->programmed;
    }
  else if (pending & (OTG_HCINT_NAK | OTG_HCINT_NYET))
    {
      result = -EAGAIN;
    }

  /* Preserve a timeout/cancel or earlier terminal cause while waiting for
   * halt. A late XFRC cannot turn an already-cancelled request into success.
   */

  if (xfer->phase == S31_HC_ACTIVE && result != -EINPROGRESS)
    {
      xfer->result = result;
      xfer->phase = S31_HC_STOPPING;
      if (!(pending & OTG_HCINT_CHH))
        {
          s31_hc_halt(io, channel);
        }
    }

  if (xfer->phase == S31_HC_STOPPING && xfer->result == 0 &&
      result < 0 && result != -EINPROGRESS && result != -EAGAIN)
    {
      xfer->result = result;
    }

  if (pending & OTG_HCINT_CHH)
    {
      if (xfer->result == -EINPROGRESS)
        {
          xfer->result = -EIO;
        }

      xfer->phase = S31_HC_DONE;
      io->write(io->arg, ESP32S31_OTG_HCINTMSK_OFFSET(channel), 0);
      return true;
    }

  return false;
}
/* Synchronous callers may return before hardware acknowledges a halt. Reap
 * that late acknowledgement under the same PIO lock before reuse/free.
 * CHENA clear alone is insufficient: the old transfer must report CHH. */
static inline bool s31_hc_reusable(struct s31_hc_transfer_s *xfer,
                                   const struct s31_hc_io_s *io,
                                   unsigned int channel)
{
  if (channel >= 16 ||
      (io->read(io->arg, ESP32S31_OTG_HCCHAR_OFFSET(channel)) & (1u << 31)))
    return false;
  if (xfer->phase == S31_HC_STOPPING &&
      (io->read(io->arg, ESP32S31_OTG_HCINT_OFFSET(channel)) & OTG_HCINT_CHH))
    s31_hc_interrupt(xfer, io, channel);
  return xfer->phase != S31_HC_ACTIVE && xfer->phase != S31_HC_STOPPING;
}
#endif
