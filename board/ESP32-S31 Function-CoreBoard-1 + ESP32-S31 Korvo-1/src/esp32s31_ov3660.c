/****************************************************************************
 * SPDX-License-Identifier: Apache-2.0
 * OV3660 SCCB initialization for the S31 fixed VGA RGB565 DVP profile.
 * Register sequences follow Espressif esp32-camera; see ov3660.NOTICE.
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/signal.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <syslog.h>
#include "esp32s31_ov3660_defaults.h"
#ifdef CONFIG_ESP32S31_VISION
static struct i2c_master_s *vision_sensor_bus;
# define OV3660_PLL_MULT 0x09
#else
# define OV3660_PLL_MULT 0x04
#endif

#if !defined(CONFIG_ESP32S31_CAMERA_DVP) || \
    CONFIG_ESP32S31_CAMERA_DVP_HRES != 640 || \
    CONFIG_ESP32S31_CAMERA_DVP_VRES != 480 || \
    CONFIG_ESP32S31_CAMERA_DVP_XCLK_FREQ != 16000000
#  error "OV3660 profile requires VGA RGB565 DVP with 16 MHz XCLK"
#endif

#define OV3660_ADDR 0x3c
#define OV3660_FREQ 100000

static int ov3660_write(struct i2c_master_s *i2c, uint16_t reg,
                        uint8_t value)
{
  uint8_t data[3] = {reg >> 8, reg & 0xff, value};
  struct i2c_config_s config =
    { .frequency = OV3660_FREQ, .address = OV3660_ADDR, .addrlen = 7 };
  return i2c_write(i2c, &config, data, sizeof(data));
}

static int ov3660_read(struct i2c_master_s *i2c, uint16_t reg)
{
  uint8_t address[2] = {reg >> 8, reg & 0xff};
  uint8_t value;
  struct i2c_msg_s msgs[2] =
  {
    { .frequency = OV3660_FREQ, .addr = OV3660_ADDR, .flags = 0,
      .buffer = address, .length = 2 },
    { .frequency = OV3660_FREQ, .addr = OV3660_ADDR, .flags = I2C_M_READ,
      .buffer = &value, .length = 1 }
  };
  int ret = I2C_TRANSFER(i2c, msgs, 2);
  return ret < 0 ? ret : value;
}

static int ov3660_table(struct i2c_master_s *i2c,
                        const uint16_t table[][2], size_t count)
{
  size_t i;
  int ret;
  for (i = 0; i < count; i++)
    {
      if (table[i][0] == 0)
        {
          break;
        }

      if (table[i][0] == 0xffff)
        {
          nxsig_usleep((unsigned int)table[i][1] * 1000);
          continue;
        }

      ret = ov3660_write(i2c, table[i][0], table[i][1]);
      if (ret < 0)
        {
          return ret;
        }
    }

  return 0;
}

int esp32s31_ov3660_initialize(struct i2c_master_s *i2c)
{
  /* Official 4:3 crop, binned/scaled VGA, no flip/mirror; VGA non-JPEG
   * Independent vision profile: multiplier 9, sysdiv 1, prediv 0,
   * seld5 2, PCLK divider 2 => SYSCLK/PCLK 18 MHz at XCLK=16 MHz.
   * Timing totals unchanged. The old multiplier=4 profile is ~4.44 FPS.
   * Approximately 10 sensor FPS; verify host timing and visual integrity.
   * Register meanings: Espressif esp32-camera sensors/ov3660.c set_pll().
   */

  static const uint16_t vga[][2] =
  {
    {0x3a0f, 59}, {0x3a10, 50}, {0x3a1b, 59},
    {0x3a1e, 50}, {0x3a11, 118}, {0x3a1f, 25},
    {0x501f, 0x01}, {0x4300, 0x61},
    {0x3800, 0x00}, {0x3801, 0x00},
    {0x3802, 0x00}, {0x3803, 0x00},
    {0x3804, 0x08}, {0x3805, 0x1f},
    {0x3806, 0x06}, {0x3807, 0x0b},
    {0x3808, 0x02}, {0x3809, 0x80},
    {0x380a, 0x01}, {0x380b, 0xe0},
    {0x380c, 0x08}, {0x380d, 0xfc},
    {0x380e, 0x03}, {0x380f, 0x0f},
    {0x3810, 0x00}, {0x3811, 0x08},
    {0x3812, 0x00}, {0x3813, 0x02},
    {0x3820, 0x01}, {0x3821, 0x01}, {0x4514, 0xaa},
    {0x4520, 0x0b}, {0x3814, 0x31}, {0x3815, 0x31},
    {0x303a, 0x00}, {0x303b, OV3660_PLL_MULT}, {0x303c, 0x11},
    {0x303d, 0x02}, {0x3824, 0x02}, {0x460c, 0x22}
  };
  static const uint16_t verify[][2] =
  {
    {0x3808, 0x02}, {0x3809, 0x80}, {0x380a, 0x01}, {0x380b, 0xe0},
    {0x501f, 0x01}, {0x4300, 0x61}, {0x303b, OV3660_PLL_MULT}
  };
  size_t i;
  int high;
  int low;
  int ret;

  if (i2c == NULL)
    {
      return -EINVAL;
    }

#ifdef CONFIG_ESP32S31_VISION
  vision_sensor_bus=i2c;
#endif
  high = ov3660_read(i2c, 0x300a);
  if (high < 0)
    {
      return high;
    }

  low = ov3660_read(i2c, 0x300b);
  if (low < 0)
    {
      return low;
    }

  syslog(LOG_INFO, "S31 camera SCCB sensor ID: 0x%04x\n",
         (high << 8) | low);

  if (((high << 8) | low) != 0x3660)
    {
      return -ENODEV;
    }

  ret = ov3660_write(i2c, 0x3008, 0x82);
  if (ret < 0)
    {
      return ret;
    }

  nxsig_usleep(100000);
  ret = ov3660_table(i2c, g_ov3660_defaults,
                    sizeof(g_ov3660_defaults) / sizeof(g_ov3660_defaults[0]));
  if (ret < 0)
    {
      return ret;
    }

  nxsig_usleep(100000);
  ret = ov3660_table(i2c, vga, sizeof(vga) / sizeof(vga[0]));
  if (ret < 0)
    {
      return ret;
    }

  /* Preserve unrelated ISP bits while enabling the VGA scaler. */

  ret = ov3660_read(i2c, 0x5001);
  if (ret < 0)
    {
      return ret;
    }

  ret = ov3660_write(i2c, 0x5001, ret | 0x20);
  if (ret < 0)
    {
      return ret;
    }

  for (i = 0; i < sizeof(verify) / sizeof(verify[0]); i++)
    {
      ret = ov3660_read(i2c, verify[i][0]);
      if (ret < 0)
        {
          return ret;
        }

      if (ret != verify[i][1])
        {
          return -EIO;
        }
    }

  return 0;
}

#ifdef CONFIG_ESP32S31_VISION
void s31vision_sensor_stats(void)
{
  struct i2c_master_s *b=vision_sensor_bus;
  syslog(LOG_INFO,"VISION SENSOR HTS=%02x%02x VTS=%02x%02x exposure=%02x%02x%02x extra=%02x%02x\n",
    ov3660_read(b,0x380c),ov3660_read(b,0x380d),ov3660_read(b,0x380e),ov3660_read(b,0x380f),
    ov3660_read(b,0x3500),ov3660_read(b,0x3501),ov3660_read(b,0x3502),
    ov3660_read(b,0x350c),ov3660_read(b,0x350d));
}

#endif
