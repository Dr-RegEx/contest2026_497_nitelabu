/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <nuttx/i2c/i2c_master.h>
#include "espressif/esp_gpio.h"

#define CODEC_ADDRESS 0x10

static int codec_read(int fd, uint32_t hz, uint8_t reg, uint8_t *value,
                       bool restart)
{
  struct i2c_msg_s msgs[2] =
  {
    {.frequency = hz, .addr = CODEC_ADDRESS,
     .flags = restart ? I2C_M_NOSTOP : 0, .buffer = &reg, .length = 1},
    {.frequency = hz, .addr = CODEC_ADDRESS,
     .flags = I2C_M_READ, .buffer = value, .length = 1}
  };
  struct i2c_transfer_s transfer = {.msgv = msgs, .msgc = 2};
  return ioctl(fd, I2CIOC_TRANSFER, (unsigned long)&transfer) < 0 ?
         -errno : OK;
}

static int codec_write(int fd, uint32_t hz, uint8_t reg, uint8_t value,
                        bool split)
{
  uint8_t data[2] = {reg, value};
  struct i2c_msg_s msgs[2] =
  {
    {.frequency = hz, .addr = CODEC_ADDRESS,
     .flags = 0, .buffer = data, .length = split ? 1 : 2},
    {.frequency = hz, .addr = CODEC_ADDRESS,
     .flags = I2C_M_NOSTART, .buffer = &data[1], .length = 1}
  };
  struct i2c_transfer_s transfer = {.msgv = msgs, .msgc = split ? 2 : 1};
  return ioctl(fd, I2CIOC_TRANSFER, (unsigned long)&transfer) < 0 ?
         -errno : OK;
}

static int codec_id(int fd, int phase, uint32_t hz)
{
  uint8_t high = 0;
  uint8_t low = 0;
  int ret = codec_read(fd, hz, 0xfd, &high, true);
  if (ret == OK)
    {
      ret = codec_read(fd, hz, 0xfe, &low, false);
    }

  if (ret == OK && (high != 0x83 || low != 0x89))
    {
      ret = -ENODEV;
    }

  printf("S31LPI2C id phase=%d hz=%lu value=%02x%02x ret=%d\n",
         phase, (unsigned long)hz, high, low, ret);
  return ret;
}

static int codec_round(int fd, int round, uint32_t hz, uint8_t reg,
                        uint8_t value, bool split)
{
  uint8_t old;
  uint8_t observed = 0;
  uint8_t restored = 0;
  int cleanup;
  int ret = codec_read(fd, hz, reg, &old, true);
  if (ret < 0)
    {
      return ret;
    }

  ret = codec_write(fd, hz, reg, value, split);
  if (ret == OK)
    {
      ret = codec_read(fd, hz, reg, &observed, true);
      if (ret == OK && observed != value)
        {
          ret = -EIO;
        }
    }

  /* Restore even if the attempted write or comparison failed. */

  cleanup = codec_write(fd, hz, reg, old, false);
  if (cleanup == OK)
    {
      cleanup = codec_read(fd, hz, reg, &restored, true);
      if (cleanup == OK && restored != old)
        {
          cleanup = -EIO;
        }
    }

  if (cleanup < 0)
    {
      printf("S31LPI2C restore-failed reg=%02x ret=%d\n", reg, cleanup);
      return cleanup;
    }

  if (ret == OK)
    {
      printf("S31LPI2C round=%d hz=%lu reg=%02x value=%02x match=1 restored=1\n",
             round, (unsigned long)hz, reg, observed);
    }
  else
    {
      printf("S31LPI2C write-failed reg=%02x old=%02x expected=%02x "
             "observed=%02x restored=%02x ret=%d\n",
             reg, old, value, observed, restored, ret);
    }

  return ret;
}

int esp32s31_lpi2c_test(void)
{
  struct i2c_msg_s probe =
  {
    .frequency = 100000, .addr = 0x7e, .flags = 0, .length = 0
  };
  struct i2c_transfer_s transfer = {.msgv = &probe, .msgc = 1};
  uint8_t control = 0;
  uint8_t active = 0;
  uint8_t restored = 0;
  uint8_t isolation = 0;
  uint8_t isolation_active = 0;
  uint8_t isolation_restored = 0;
  bool have_control = false;
  bool have_isolation = false;
  int cleanup;
  int fd;
  int ret;

  esp_gpiowrite(7, false);
  ret = esp_configgpio(7, OUTPUT);
  if (ret < 0)
    {
      return ret;
    }

  fd = open("/dev/i2c2", O_RDWR);
  if (fd < 0)
    {
      return -errno;
    }

  ret = codec_id(fd, 0, 100000);
  if (ret == OK)
    {
      ret = codec_read(fd, 100000, 0xf3, &isolation, true);
      if (ret == OK && (isolation & 0x80) && !(isolation & 0x01))
        {
          /* Isolated, live DVDD settings cannot be saved through I2C.
           * Accept an accessible domain or the original reset-held state.
           */

          ret = -ENOTSUP;
        }

      have_isolation = ret == OK;
    }

  /* The codec datasheet requires F3=0 before DVDD register access. IDs and
   * F3 bits 7/4/2/0 are in PVDD and remain readable while DVDD is isolated.
   */

  if (ret == OK)
    {
      ret = codec_write(fd, 100000, 0xf3, 0x00, false);
      if (ret == OK)
        {
          ret = codec_read(fd, 100000, 0xf3, &isolation_active, true);
          if (ret == OK && (isolation_active & 0x91))
            {
              ret = -EIO;
            }
        }

      printf("S31LPI2C isolation before=%02x active=%02x ret=%d\n",
             isolation, isolation_active, ret);
    }

  if (ret == OK)
    {
      ret = codec_read(fd, 100000, 0x00, &control, true);
      have_control = ret == OK;
    }

  /* Power-on RST_REGS holds writable registers at their defaults. Release
   * only that bit; retain all digital/clock resets and restore it below.
   */

  if (ret == OK)
    {
      ret = codec_write(fd, 100000, 0x00, control & 0x7f, false);
      if (ret == OK)
        {
          ret = codec_read(fd, 100000, 0x00, &active, true);
          if (ret == OK && active != (control & 0x7f))
            {
              ret = -EIO;
            }
        }

      printf("S31LPI2C control before=%02x active=%02x ret=%d\n",
             control, active, ret);
    }

  if (ret == OK)
    {
      ret = codec_round(fd, 0, 100000, 0x46, 0x5a, false);
    }

  if (ret == OK)
    {
      ret = codec_round(fd, 1, 100000, 0x47, 0xa5, false);
    }

  if (ret == OK)
    {
      ret = codec_round(fd, 2, 400000, 0x46, 0xa5, true);
    }

  if (ret == OK)
    {
      ret = codec_round(fd, 3, 400000, 0x47, 0x5a, true);
    }

  if (ret == OK)
    {
      errno = 0;
      if (ioctl(fd, I2CIOC_TRANSFER, (unsigned long)&transfer) != -1 ||
          errno != ENXIO)
        {
          ret = -EIO;
        }
    }

  if (ret == OK)
    {
      ret = codec_id(fd, 1, 400000);
    }

  if (ret == OK)
    {
      ret = ioctl(fd, I2CIOC_RESET, 0) < 0 ? -errno : OK;
    }

  if (ret == OK)
    {
      ret = codec_id(fd, 2, 100000);
    }

  if (have_control)
    {
      cleanup = codec_write(fd, 100000, 0x00, control, false);
      if (cleanup == OK)
        {
          cleanup = codec_read(fd, 100000, 0x00, &restored, true);
          if (cleanup == OK && restored != control)
            {
              cleanup = -EIO;
            }
        }

      printf("S31LPI2C control restored=%02x ret=%d\n", restored, cleanup);
      if (cleanup < 0)
        {
          ret = cleanup;
        }
    }

  if (have_isolation)
    {
      cleanup = codec_write(fd, 100000, 0xf3, isolation, false);
      if (cleanup == OK)
        {
          cleanup = codec_read(fd, 100000, 0xf3, &isolation_restored, true);

          /* Bits 7/4/0 are PVDD controls; bit 2 is a status flag. DVDD
           * bits cannot be compared while the original isolation is on.
           * If originally reset-held, DVDD settings return to defaults.
           */

          if (cleanup == OK &&
              ((isolation_restored ^ isolation) &
               ((isolation & 0x80) ? 0x91 : 0xf9)))
            {
              cleanup = -EIO;
            }
        }

      printf("S31LPI2C isolation restored=%02x ret=%d\n",
             isolation_restored, cleanup);
      if (cleanup < 0)
        {
          ret = cleanup;
        }
    }

  if (close(fd) < 0 && ret == OK)
    {
      ret = -errno;
    }

  if (ret < 0)
    {
      return ret;
    }

  fd = open("/dev/i2c2", O_RDWR);
  if (fd < 0)
    {
      return -errno;
    }

  ret = codec_id(fd, 3, 400000);
  if (close(fd) < 0 && ret == OK)
    {
      ret = -errno;
    }

  if (ret == OK)
    {
      printf("S31LPI2C PASS writes=4 ids=4 nack=ENXIO reset=1 reopen=1\n");
    }

  return ret;
}
