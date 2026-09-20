/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_rtc_periodic.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_RTC_PERIODIC_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_RTC_PERIODIC_H

#include <nuttx/config.h>
#include <nuttx/timers/rtc.h>

#ifdef CONFIG_RTC_PERIODIC
int esp32s31_rtc_setperiodic(struct rtc_lowerhalf_s *lower,
                            const struct lower_setperiodic_s *info);
int esp32s31_rtc_cancelperiodic(struct rtc_lowerhalf_s *lower, int id);
#endif

#endif /* __ARCH_RISCV_SRC_ESP32S31_ESP32S31_RTC_PERIODIC_H */
