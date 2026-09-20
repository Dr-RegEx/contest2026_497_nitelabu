/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_BLE_DISPATCH_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_BLE_DISPATCH_H

/* Arguments must reside in kernel memory and remain live until return. */

int esp32s31_ble_dispatch_init(void);
int esp32s31_ble_dispatch(int (*function)(void *), void *argument);

#endif
