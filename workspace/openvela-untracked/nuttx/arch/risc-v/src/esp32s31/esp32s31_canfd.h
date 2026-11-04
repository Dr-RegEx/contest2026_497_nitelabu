/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_CANFD_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_CANFD_H

/* Register CAN0 as /dev/can0. CONFIG_CAN_LOOPBACK requires the same valid,
 * board-reserved GPIO for TX and RX, with no external wire or transceiver.
 * Normal bus operation requires distinct GPIOs and a CAN FD transceiver.
 */

int esp32s31_canfd_initialize(int tx_pin, int rx_pin);

#endif
