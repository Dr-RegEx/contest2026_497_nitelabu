/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_LPI2C_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_LPI2C_H

#include <nuttx/i2c/i2c_master.h>

/* Own LP I2C0 exclusively, using two distinct board-reserved LP GPIOs 0..7.
 * Returns one initialized reference, or NULL with errno set. Repeated calls
 * must use the same pins. Balance each reference with uninitialize.
 * The standard setup/shutdown methods also manage references, so a registered
 * I2C character device can power the peripheral only while it is open.
 */

struct i2c_master_s *esp32s31_lpi2cbus_initialize(int scl, int sda);
int esp32s31_lpi2cbus_uninitialize(struct i2c_master_s *dev);

#endif
