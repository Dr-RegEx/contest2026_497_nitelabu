/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_wdt.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_WDT_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_WDT_H

int esp32s31_wdt_initialize(const char *path);

#ifdef CONFIG_ESP32S31_WDT_ROM_DELAY
int esp32s31_wdt_delay_check(void);
#endif

#endif
