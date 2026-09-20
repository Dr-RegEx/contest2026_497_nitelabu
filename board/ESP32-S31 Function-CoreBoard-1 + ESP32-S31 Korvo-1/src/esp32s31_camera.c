/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_camera.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <syslog.h>

#include <nuttx/i2c/i2c_master.h>
#ifdef CONFIG_ESP32S31_CAMERA_OV2640
#  include <nuttx/video/ov2640.h>
#else
int esp32s31_ov3660_initialize(struct i2c_master_s *i2c);
#endif

#include "espressif/esp_i2c.h"

#ifdef CONFIG_ESP32S31_CAMERA_DVP
#  include "esp32s31_camera_dvp.h"
#endif

#ifdef CONFIG_ESP32S31_CAMERA_V4L2
int esp32s31_camera_v4l2_initialize(void);
#endif

#if !defined(CONFIG_ESPRESSIF_I2C0) || \
    !defined(CONFIG_I2C_DRIVER) || \
    CONFIG_ESPRESSIF_I2C0_SCLPIN != 1 || \
    CONFIG_ESPRESSIF_I2C0_SDAPIN != 0
#  error "Camera fixture requires I2C0 SCCB on GPIO1/GPIO0"
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int esp32s31_camera_initialize(void)
{
  struct i2c_master_s *i2c;
  int ret;

  /* The board bring-up has already registered I2C0.  Reusing the bus here
   * keeps the probe isolated and avoids creating a second device node. */

  i2c = esp_i2cbus_initialize(0);
  if (i2c == NULL)
    {
      return -ENODEV;
    }

#ifdef CONFIG_ESP32S31_CAMERA_DVP
  /* The sensor needs XCLK before SCCB probing/reset.  Controller creation
   * routes and starts XCLK, but DMA capture is not started here. */

  ret = esp32s31_camera_dvp_initialize();
  if (ret < 0)
    {
      return ret;
    }
#endif

#ifdef CONFIG_ESP32S31_CAMERA_OV3660
  ret = esp32s31_ov3660_initialize(i2c);
#else
  ret = ov2640_initialize(i2c);
#endif
  if (ret < 0)
    {
      syslog(LOG_WARNING, "Camera SCCB probe/config failed: %d\n", ret);
#ifdef CONFIG_ESP32S31_CAMERA_DVP
      esp32s31_camera_dvp_uninitialize();
#endif
      return ret;
    }

#ifdef CONFIG_ESP32S31_CAMERA_V4L2
  if (ret >= 0)
    {
      ret = esp32s31_camera_v4l2_initialize();
      if (ret < 0)
        {
          syslog(LOG_WARNING, "S31 V4L2 capture registration failed: %d\n",
                 ret);
          esp32s31_camera_dvp_uninitialize();
        }
    }
#endif

  return ret;
}
