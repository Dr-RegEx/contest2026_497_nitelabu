/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_LPUART_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_LPUART_H

#include <stdbool.h>

/* Register /dev/ttyLP0. External operation requires two distinct reserved
 * LP GPIOs (0..7). Internal loopback requires txpin = rxpin = -1 and does
 * not alter GPIO routing. This driver exclusively owns LP UART0.
 */

int esp32s31_lpuart_initialize(int txpin, int rxpin, bool loopback);

#endif
