/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/include/board.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __BOARDS_RISCV_ESP32S31_CORE_FUNCTION_BOARD_INCLUDE_BOARD_H
#define __BOARDS_RISCV_ESP32S31_CORE_FUNCTION_BOARD_INCLUDE_BOARD_H

#include <nuttx/config.h>

/* Old frozen configurations predate the explicit variant choice. Preserve
 * their board selection while keeping new headless Korvo profiles independent
 * of the framebuffer flag. Do not alter generated CONFIG symbols here.
 */

#if defined(CONFIG_ESP32S31_BOARD_KORVO) || \
    (!defined(CONFIG_ESP32S31_BOARD_FUNCTION) && \
     defined(CONFIG_ESP32S31_KORVO_RGB565))
#  define BOARD_IS_KORVO    1
#  define BOARD_NAME        "ESP32-S31-Korvo-1"
#  define BOARD_RGBLED_PIN  37
#  define BOARD_SD_POWER_PIN 39
#else
#  define BOARD_IS_KORVO    0
#  define BOARD_NAME        "ESP32-S31-Function-CoreBoard-1"
#  define BOARD_RGBLED_PIN  60
#endif

/* Function RGMII group A, matching the locked IDF board example. */

#if !BOARD_IS_KORVO
#  define BOARD_ETH_MDC       5
#  define BOARD_ETH_MDIO      6
#  define BOARD_ETH_RESET     7
#  define BOARD_ETH_TXD0      8
#  define BOARD_ETH_TXD1      9
#  define BOARD_ETH_TXD2      10
#  define BOARD_ETH_TXD3      11
#  define BOARD_ETH_TX_CTL    12
#  define BOARD_ETH_TX_CLK    13
#  define BOARD_ETH_RX_CLK    14
#  define BOARD_ETH_RX_CTL    15
#  define BOARD_ETH_RXD0      19
#  define BOARD_ETH_RXD1      18
#  define BOARD_ETH_RXD2      17
#  define BOARD_ETH_RXD3      16
#endif

/* The board's ROM/bootloader console routes UART0 to these pins. */

#define BOARD_UART0_TX_PIN 58
#define BOARD_UART0_RX_PIN 59

/* Korvo GPIO61 is also LCD SCK. ADC42 keys use their own standard button
 * lower half; they do not use the Function board's GPIO BOOT driver.
 */

#if !BOARD_IS_KORVO
/* BOOT is an active-low input. Never drive this strapping pin. */
#define BOARD_BUTTON_BOOT_PIN 61
#define BUTTON_BOOT           0
#define BUTTON_BOOT_BIT       (1 << BUTTON_BOOT)
#define NUM_BUTTONS           1
#elif defined(CONFIG_ESP32S31_KORVO_ADC_BUTTONS)
#define BOARD_ADC_BUTTON_PIN  42
#define BUTTON_PLAY           0
#define BUTTON_SET            1
#define BUTTON_VOLDOWN        2
#define BUTTON_VOLUP          3
#define BUTTON_PLAY_BIT       (1 << BUTTON_PLAY)
#define BUTTON_SET_BIT        (1 << BUTTON_SET)
#define BUTTON_VOLDOWN_BIT    (1 << BUTTON_VOLDOWN)
#define BUTTON_VOLUP_BIT      (1 << BUTTON_VOLUP)
#define NUM_BUTTONS           4
#else
#define NUM_BUTTONS           0
#endif

#endif /* __BOARDS_RISCV_ESP32S31_CORE_FUNCTION_BOARD_INCLUDE_BOARD_H */
