/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_bmi160_uorb.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <syslog.h>

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/sensors/bmi160.h>

#include "espressif/esp_i2c.h"

/* This fixture image routes I2C0 to the free J2 header pins.  The original
 * category Sensor test subscribes to both uncalibrated uORB topics.
 */

#if !defined(CONFIG_ESPRESSIF_I2C0) || \
    !defined(CONFIG_SENSORS_BMI160_I2C) || \
    !defined(CONFIG_SENSORS_BMI160_UORB) || \
    !defined(CONFIG_BMI160_I2C_ADDR_68)
#  error "BMI160 xTS requires I2C0, address 0x68 and the uORB interface"
#endif

#if CONFIG_ESPRESSIF_I2C0_SCLPIN != 45 || \
    CONFIG_ESPRESSIF_I2C0_SDAPIN != 46
#  error "BMI160 fixture requires GPIO45 SCL and GPIO46 SDA"
#endif

int esp32s31_bmi160_uorb_initialize(void)
{
  struct i2c_master_s *i2c;
  int ret;

  i2c = esp_i2cbus_initialize(0);
  if (i2c == NULL)
    {
      return -ENODEV;
    }

  /* bmi160_register_uorb probes CHIP_ID before publishing the topics.  An
   * absent fixture must remain a registration failure, never a test pass.
   */

  ret = bmi160_register_uorb(0, i2c);
  if (ret < 0)
    {
      /* Keep the bus alive if partial registration could not be unwound.
       * This optional fixture is initialized once during board bringup.
       */
      syslog(LOG_ERR, "xTS BMI160 uORB: registration failed: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO,
         "xTS BMI160 uORB: accel_uncal0/gyro_uncal0 "
         "I2C0 addr=0x68 SCL=45 SDA=46\n");
  return 0;
}
