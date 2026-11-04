/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_es8389.c
 *
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * Register sequences derived from esp-audio-dev, commit
 * a9e2eafb45c41ab313d720adc20f6785f4bd10ab, device/es8389/es8389.c.
 * See es8389-LICENSE. This backend is exclusively for Espressif products.
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/signal.h>
#include <errno.h>
#include <stddef.h>
#include "esp32s31_es8389.h"

struct es8389_reg_s
{
  uint8_t reg;
  uint8_t value;
};

/* Explicitly muted during initialization. Differential MIC1/MIC2 inputs,
 * normal analog drive, no DAC-reference channel, codec slave. The two-slot
 * Philips frame is supported by the S31 TDM controller as well as std mode.
 */

static const struct es8389_reg_s g_init[] =
{
  {0xf3, 0x00}, {0x00, 0x7e}, {0xf3, 0x38},
  {0x24, 0x64}, {0x25, 0x04}, {0x45, 0x03},
  {0x60, 0x2a}, {0x61, 0xc9}, {0x62, 0x4f}, {0x63, 0x06},
  {0x6b, 0x00}, {0x6d, 0x16}, {0x6e, 0xaa}, {0x6f, 0x66}, {0x70, 0x99},
  {0x23, 0x00}, {0x72, 0x10}, {0x73, 0x10},
  {0x10, 0xc4}, {0x01, 0x08}, {0xf1, 0x00},
  {0x12, 0x01}, {0x13, 0x01}, {0x14, 0x01}, {0x15, 0x01},
  {0x16, 0x12}, {0x17, 0x09}, {0x18, 0x31}, {0x19, 0x0e},
  {0x1a, 0x01}, {0x1b, 0x01}, {0x1c, 0x11}, {0x2a, 0x00},
  {0x20, 0x63}, {0x40, 0x63}, {0xf0, 0x12}, {0x02, 0x40},

  /* Exact coefficient row: sysclk=BCLK=512000, LRCK=16000, ratio=32.
   * Never substitute the legacy no-MCLK fallback of fs*bits*4 (64fs).
   */

  {0x04, 0x00}, {0x05, 0x55}, {0x06, 0x84}, {0x07, 0xd0},
  {0x08, 0x01}, {0x09, 0xc1}, {0x0a, 0x90}, {0x0b, 0x04},
  {0x0c, 0x00}, {0x0d, 0x20}, {0x0f, 0x10}, {0x11, 0x00},
  {0x21, 0x1f}, {0x22, 0x7f}, {0x26, 0xbf}, {0x27, 0xbf},
  {0x28, 0xbf}, {0x2f, 0xc0}, {0x30, 0xf4}, {0x31, 0x00},
  {0x44, 0x00}, {0x41, 0xff}, {0x42, 0x7f}, {0x43, 0x10},
  {0x49, 0x0f}, {0x4c, 0xc0}, {0x00, 0x00}, {0x03, 0xc1},
  {0x00, 0x01}, {0x4d, 0x00}, {0x48, 0xbe}
};

static const struct es8389_reg_s g_bias[] =
{
  {0x61, 0xd9}, {0x64, 0x8f}, {0x10, 0xe4}, {0x00, 0x01},
  {0x03, 0xc3}, {0x24, 0x6a}, {0x25, 0x0a}
};

static int codec_write(struct s31_es8389_s *codec, uint8_t reg,
                       uint8_t value)
{
  uint8_t data[2] = {reg, value};
  struct i2c_msg_s msg =
  {
    .frequency = 100000,
    .addr = S31_ES8389_ADDRESS,
    .flags = 0,
    .buffer = data,
    .length = sizeof(data)
  };
  int ret = I2C_TRANSFER(codec->i2c, &msg, 1);

  /* The pinned Espressif master returns OK; other NuttX masters return the
   * number of messages. Accept either complete-success convention.
   */

  return ret < 0 ? ret : (ret == 0 || ret == 1 ? OK : -EIO);
}

static int codec_update(struct s31_es8389_s *codec, uint8_t reg,
                        uint8_t mask, uint8_t value)
{
  uint8_t old;
  struct i2c_msg_s msg[2] =
  {
    {.frequency = 100000, .addr = S31_ES8389_ADDRESS,
     .flags = 0, .buffer = &reg, .length = 1},
    {.frequency = 100000, .addr = S31_ES8389_ADDRESS,
     .flags = I2C_M_READ, .buffer = &old, .length = 1}
  };
  int ret = I2C_TRANSFER(codec->i2c, msg, 2);

  if (ret < 0 || (ret != 0 && ret != 2))
    {
      return ret < 0 ? ret : -EIO;
    }

  return codec_write(codec, reg, (old & ~mask) | (value & mask));
}

static int codec_sequence(struct s31_es8389_s *codec,
                          const struct es8389_reg_s *seq, size_t count)
{
  size_t i;
  int ret;

  for (i = 0; i < count; i++)
    {
      ret = codec_write(codec, seq[i].reg, seq[i].value);
      if (ret < 0)
        {
          return ret;
        }
    }

  return OK;
}

static int codec_bias(struct s31_es8389_s *codec)
{
  int ret = codec_write(codec, 0x4d, 0x00);

  if (ret == OK)
    {
      ret = codec_update(codec, 0x69, 0x20, 0x20);
    }

  if (ret == OK)
    {
      ret = codec_sequence(codec, g_bias,
                           sizeof(g_bias) / sizeof(g_bias[0]));
    }

  return ret;
}

static int codec_volume(struct s31_es8389_s *codec, uint16_t volume)
{
  uint8_t reg = (uint32_t)volume * 191 / 1000;
  int ret = codec_write(codec, 0x46, reg);

  return ret < 0 ? ret : codec_write(codec, 0x47, reg);
}

static int codec_fault(struct s31_es8389_s *codec, int error)
{
  /* A shared control error must not leave an amplifier enabled. Keep the
   * active ownership bits: neither a retry nor a second client may reset
   * the physical codec while another stream still owns it.
   */

  codec->pa(false);
  codec->faulted = true;
  codec->ready = false;
  codec_write(codec, 0x20, 0x63);
  codec_write(codec, 0x40, 0x63);
  return error;
}

static int codec_suspend(struct s31_es8389_s *codec)
{
  static const struct es8389_reg_s before[] =
  {
    {0x20, 0x63}, {0x40, 0x63}, {0x10, 0xd4}
  };
  static const struct es8389_reg_s middle[] =
  {
    {0x61, 0x59}, {0x64, 0x00}, {0x03, 0x00}, {0x00, 0x7e},
    {0x01, 0x28}
  };
  static const struct es8389_reg_s after[] =
  {
    {0x60, 0x00}, {0x00, 0x00}, {0x10, 0xcc}
  };
  static const struct es8389_reg_s final[] =
  {
    {0x10, 0x00}, {0x61, 0x08}, {0xf3, 0xc1}, {0xf2, 0x00}
  };
  int ret;

  codec->pa(false);
  codec->ready = false;
  ret = codec_sequence(codec, before, sizeof(before) / sizeof(before[0]));
  if (ret == OK)
    {
      nxsig_usleep(70000);
      ret = codec_sequence(codec, middle,
                           sizeof(middle) / sizeof(middle[0]));
    }

  if (ret == OK)
    {
      ret = codec_update(codec, 0x69, 0x20, 0x00);
    }

  if (ret == OK)
    {
      ret = codec_sequence(codec, after, sizeof(after) / sizeof(after[0]));
    }

  if (ret == OK)
    {
      nxsig_usleep(500000);
      ret = codec_sequence(codec, final, sizeof(final) / sizeof(final[0]));
    }

  return ret < 0 ? codec_fault(codec, ret) : OK;
}

int s31_es8389_prepare(struct s31_es8389_s *codec)
{
  int ret;

  if (codec == NULL || codec->i2c == NULL || codec->pa == NULL ||
      codec->volume > 1000)
    {
      return -EINVAL;
    }

  nxmutex_lock(&codec->lock);
  if (codec->active != 0)
    {
      ret = codec->faulted ? -EIO : OK;
    }
  else if (codec->ready)
    {
      ret = OK;
    }
  else
    {
      codec->pa(false);
      ret = codec_sequence(codec, g_init, sizeof(g_init) / sizeof(g_init[0]));
      if (ret == OK)
        {
          ret = codec_volume(codec, codec->volume);
        }

      if (ret == OK)
        {
          codec->ready = true;
          codec->faulted = false;
        }
      else
        {
          ret = codec_fault(codec, ret);
        }
    }

  nxmutex_unlock(&codec->lock);
  return ret;
}

int s31_es8389_enable(struct s31_es8389_s *codec, uint8_t direction)
{
  int ret;

  if (direction != S31_ES8389_PLAYBACK && direction != S31_ES8389_CAPTURE)
    {
      return -EINVAL;
    }

  nxmutex_lock(&codec->lock);
  if (codec->active & direction)
    {
      ret = -EBUSY;
    }
  else if (!codec->ready || codec->faulted)
    {
      ret = -EIO;
    }
  else
    {
      ret = codec->active == 0 ? codec_bias(codec) : OK;
      if (ret == OK)
        {
          ret = codec_update(codec, direction == S31_ES8389_PLAYBACK ?
                              0x40 : 0x20, 0x03,
                              direction == S31_ES8389_PLAYBACK &&
                              codec->muted ? 0x03 : 0x00);
        }

      if (ret == OK)
        {
          codec->active |= direction;
          if (direction == S31_ES8389_PLAYBACK)
            {
              codec->pa(!codec->muted);
            }
        }
      else
        {
          ret = codec_fault(codec, ret);
        }
    }

  nxmutex_unlock(&codec->lock);
  return ret;
}

int s31_es8389_disable(struct s31_es8389_s *codec, uint8_t direction)
{
  int ret = OK;

  if (direction != S31_ES8389_PLAYBACK && direction != S31_ES8389_CAPTURE)
    {
      return -EINVAL;
    }

  nxmutex_lock(&codec->lock);
  if (direction == S31_ES8389_PLAYBACK)
    {
      codec->pa(false);
    }

  if (codec->active & direction)
    {
      ret = codec_update(codec, direction == S31_ES8389_PLAYBACK ?
                          0x40 : 0x20, 0x03, 0x03);
      codec->active &= ~direction;
      if (ret < 0)
        {
          codec_fault(codec, ret);
        }
    }

  if (codec->active == 0 && (codec->ready || codec->faulted))
    {
      int suspend = codec_suspend(codec);

      if (ret == OK)
        {
          ret = suspend;
        }
    }

  nxmutex_unlock(&codec->lock);
  return ret;
}

int s31_es8389_volume(struct s31_es8389_s *codec, uint16_t volume)
{
  int ret = OK;

  if (volume > 1000)
    {
      return -ERANGE;
    }

  nxmutex_lock(&codec->lock);
  if (codec->faulted)
    {
      ret = -EIO;
    }
  else if (codec->ready)
    {
      ret = codec_volume(codec, volume);
      if (ret < 0)
        {
          codec_fault(codec, ret);
        }
    }

  if (ret == OK)
    {
      codec->volume = volume;
    }

  nxmutex_unlock(&codec->lock);
  return ret;
}

int s31_es8389_mute(struct s31_es8389_s *codec, bool mute)
{
  int ret = OK;

  nxmutex_lock(&codec->lock);
  if (mute)
    {
      codec->pa(false);
    }

  if (codec->faulted)
    {
      ret = -EIO;
    }
  else if (codec->ready)
    {
      ret = codec_update(codec, 0x40, 0x03,
                          mute || !(codec->active & S31_ES8389_PLAYBACK) ?
                          0x03 : 0x00);
      if (ret < 0)
        {
          codec_fault(codec, ret);
        }
    }

  if (ret == OK)
    {
      codec->muted = mute;
      if (codec->active & S31_ES8389_PLAYBACK)
        {
          codec->pa(!mute);
        }
    }

  nxmutex_unlock(&codec->lock);
  return ret;
}

int s31_es8389_health(struct s31_es8389_s *codec)
{
  int ret;

  nxmutex_lock(&codec->lock);
  ret = codec->faulted ? -EIO : OK;
  nxmutex_unlock(&codec->lock);
  return ret;
}
