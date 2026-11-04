/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_heap.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_HEAP_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_HEAP_H

#include <stddef.h>

void esp32s31_addregion(void);
#ifdef CONFIG_ESP32S31_SPIRAM_SEPARATE_HEAP
void esp32s31_allocate_user_heap(void **start, size_t *size);
#endif

#endif /* __ARCH_RISCV_SRC_ESP32S31_ESP32S31_HEAP_H */
