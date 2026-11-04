/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_QENCODER_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_QENCODER_H

/* PCNT0/unit0, X4 decoding. Both pins must be valid GPIOs, or both -1
 * to leave the inputs disconnected from external pads. The caller owns
 * board pin allocation. Position is bounded by the signed 16-bit counter;
 * no interrupt-driven overflow accumulation or index input is provided.
 */

int esp32s31_qencoder_initialize(const char *path, int pin_a, int pin_b);

#endif
