/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/include/board.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __BOARDS_RISCV_ESP32S31_CORE_FUNCTION_BOARD_INCLUDE_BOARD_H
#define __BOARDS_RISCV_ESP32S31_CORE_FUNCTION_BOARD_INCLUDE_BOARD_H

/* The board's ROM/bootloader console routes UART0 to these pins. */

#define BOARD_UART0_TX_PIN 58
#define BOARD_UART0_RX_PIN 59

/* BOOT is an active-low input.  Never drive this strapping pin. */

#define BOARD_BUTTON_BOOT_PIN 61
#define BUTTON_BOOT           0
#define BUTTON_BOOT_BIT       (1 << BUTTON_BOOT)
#define NUM_BUTTONS           1

#endif /* __BOARDS_RISCV_ESP32S31_CORE_FUNCTION_BOARD_INCLUDE_BOARD_H */
