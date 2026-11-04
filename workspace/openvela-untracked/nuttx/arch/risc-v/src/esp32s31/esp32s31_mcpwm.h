/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_MCPWM_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_MCPWM_H

/* Own MCPWM0, timer/operator/generator 0 and capture channel 0.
 * The board reserves both GPIOs. They may be the same for pad loopback.
 */

int esp32s31_mcpwm_initialize(int pwm_pin, int capture_pin);

#endif
