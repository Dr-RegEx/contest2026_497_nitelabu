/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_COMP_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_COMP_H

/* Own comparator unit 0, source channel 0. GPIO must be reserved by the
 * board. ref_tenths is 1..7 (10..70 percent of VDD), not a calibrated voltage.
 */

int esp32s31_comp_initialize(int pin, unsigned int ref_tenths);

#endif
