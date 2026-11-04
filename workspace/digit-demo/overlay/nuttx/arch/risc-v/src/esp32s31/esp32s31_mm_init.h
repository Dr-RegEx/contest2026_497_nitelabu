/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_mm_init.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_MM_INIT_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_MM_INIT_H

#include <nuttx/config.h>

#include <stdint.h>

#ifdef CONFIG_BUILD_KERNEL
void esp32s31_mm_init(void);
void esp32s31_addrenv_coherent(void);
uintptr_t esp32s31_pgtable_alloc(void);
void esp32s31_pgtable_free(uintptr_t paddr);
#endif

#endif /* __ARCH_RISCV_SRC_ESP32S31_ESP32S31_MM_INIT_H */
