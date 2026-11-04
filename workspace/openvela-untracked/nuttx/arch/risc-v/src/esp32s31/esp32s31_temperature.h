/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_temperature.h
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_TEMPERATURE_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_TEMPERATURE_H

/* Register the internal chip temperature through the standard sensor upper
 * half. Applications read sensor_temp events with O_NONBLOCK. This measures
 * the die temperature, not ambient air temperature.
 */

int esp32s31_temperature_initialize(void);

#endif
