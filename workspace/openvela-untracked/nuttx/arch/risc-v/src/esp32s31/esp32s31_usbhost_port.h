/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __ARCH_RISCV_SRC_ESP32S31_USBHOST_PORT_H
#define __ARCH_RISCV_SRC_ESP32S31_USBHOST_PORT_H
#include "esp32s31_usbhost_channel.h"

/* HPRT status is never echoed into write-one-clear/disable fields. */
static inline uint32_t s31_port_control(uint32_t value)
{
  return value & ~(OTG_HPRT_PCDET | OTG_HPRT_PENCHNG |
                   OTG_HPRT_POCCHNG | OTG_HPRT_PENA);
}

/* Ten consecutive 10-ms samples qualify insertion; removal is immediate. */
static inline bool s31_port_observe(uint32_t value, unsigned int *stable)
{
  if (!(value & OTG_HPRT_PCSTS) || (value & OTG_HPRT_POCA))
    *stable = 0;
  else if (*stable < 10)
    ++*stable;
  return *stable == 10;
}

static inline int s31_port_reset(const struct s31_hc_io_s *io,
                                void (*delay_ms)(unsigned int))
{
  uint32_t value = io->read(io->arg, ESP32S31_OTG_HPRT_OFFSET);
  unsigned int i;
  if (!(value & OTG_HPRT_PCSTS) || (value & OTG_HPRT_POCA))
    return -ENODEV;
  io->write(io->arg, ESP32S31_OTG_HPRT_OFFSET,
            s31_port_control(value) | OTG_HPRT_PRST);
  delay_ms(50);
  value = io->read(io->arg, ESP32S31_OTG_HPRT_OFFSET);
  io->write(io->arg, ESP32S31_OTG_HPRT_OFFSET,
            s31_port_control(value) & ~OTG_HPRT_PRST);
  for (i = 0; i < 100; i++)
    {
      delay_ms(10);
      value = io->read(io->arg, ESP32S31_OTG_HPRT_OFFSET);
      if (!(value & OTG_HPRT_PCSTS) || (value & OTG_HPRT_POCA))
        return -ENODEV;
      if (value & OTG_HPRT_PENA)
        {
          unsigned int speed = (value & OTG_HPRT_PSPD_MASK) >> OTG_HPRT_PSPD_SHIFT;
          return speed < 3 ? (int)speed : -EPROTO;
        }
    }
  return -ETIMEDOUT;
}
#endif
