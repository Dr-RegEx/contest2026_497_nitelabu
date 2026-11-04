/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __ARCH_RISCV_SRC_ESP32S31_USBHOST_PERIODIC_H
#define __ARCH_RISCV_SRC_ESP32S31_USBHOST_PERIODIC_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

/* HFNUM counts 1-ms frames at FS/LS and 125-us microframes at HS. Keep the
 * original interval across callbacks; resubmission must not poll early.
 */
struct s31_usb_periodic_s
{
  uint64_t launched_ms;
  uint16_t frame;
  uint16_t interval;
  bool scheduled;
};

static inline int s31_usb_periodic_interval(bool highspeed, uint8_t value)
{
  if (!value || (highspeed && value > 16))
    return -EINVAL;
  return highspeed ? 1u << (value - 1) : value;
}

static inline bool s31_usb_periodic_due(const struct s31_usb_periodic_s *state,
                                        uint16_t frame, uint64_t now_ms,
                                        bool highspeed, unsigned int tick_ms)
{
  uint32_t period_ms = ((uint32_t)state->interval *
                        (highspeed ? 125 : 1000) + 999) / 1000;

  /* The elapsed system time fallback handles one or more HFNUM wraps
   * while a client is idle. One tick of slack prevents a coarse clock from
   * making a request eligible before its descriptor interval has elapsed.
   */
  return !state->scheduled ||
         (uint16_t)(frame - state->frame) >= state->interval ||
         now_ms - state->launched_ms > period_ms + tick_ms;
}

static inline void s31_usb_periodic_record(struct s31_usb_periodic_s *state,
                                           uint16_t frame, uint64_t now_ms)
{
  state->frame = frame;
  state->launched_ms = now_ms;
  state->scheduled = true;
}
#endif
