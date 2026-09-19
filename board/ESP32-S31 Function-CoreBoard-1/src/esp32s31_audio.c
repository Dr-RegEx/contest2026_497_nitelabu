/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_audio.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <syslog.h>

#include <nuttx/audio/audio.h>
#include <nuttx/audio/es8311.h>
#include <nuttx/audio/i2s.h>
#include <nuttx/i2c/i2c_master.h>

#include "espressif/esp_gpio.h"
#include "espressif/esp_i2c.h"
#include "esp32s31_i2s.h"

#define BOARD_AUDIO_PA_PIN 57

static const struct es8311_lower_s g_codec_config =
{
  .frequency = 100000,
  .address = 0x18
};

#ifdef CONFIG_ESP32S31_I2S_DUPLEX
static const struct es8311_lower_s g_capture_config =
{
  .frequency = 100000,
  .address = 0x18,
  .capture = true
};
#endif

/* The dedicated I2S driver enables the amplifier only during playback. */

void esp32s31_audio_pa(bool enable)
{
  esp_gpiowrite(BOARD_AUDIO_PA_PIN, enable);
}

int esp32s31_audio_initialize(void)
{
  static bool initialized;
  struct audio_lowerhalf_s *playback;
  struct audio_lowerhalf_s *capture;
  struct i2c_master_s *i2c;
  struct i2s_dev_s *i2s;
  int ret;

  if (initialized)
    {
      return OK;
    }

  esp_gpiowrite(BOARD_AUDIO_PA_PIN, false);
  ret = esp_configgpio(BOARD_AUDIO_PA_PIN, OUTPUT);
  if (ret < 0)
    {
      return ret;
    }

  i2c = esp_i2cbus_initialize(0);
  if (i2c == NULL)
    {
      return -ENODEV;
    }

#ifdef CONFIG_ESP32S31_I2S_DUPLEX
  i2s = esp32s31_i2s_duplex_initialize(false);
#else
  i2s = esp32s31_i2s_initialize(0);
#endif
  if (i2s == NULL)
    {
      esp_i2cbus_uninitialize(i2c);
      return -ENODEV;
    }

  /* Each upper half needs its own codec state and callback context. Both
   * nodes share the physical codec and I2S bus; this profile is half-duplex.
   * Raw PCM nodes match the original cmocka_driver_audio device names.
   */

  playback = es8311_initialize(i2c, i2s, &g_codec_config);
#ifdef CONFIG_ESP32S31_I2S_DUPLEX
  i2s = esp32s31_i2s_duplex_initialize(true);
  capture = i2s == NULL ? NULL :
            es8311_initialize(i2c, i2s, &g_capture_config);
#else
  capture = es8311_initialize(i2c, i2s, &g_codec_config);
#endif
  if (playback == NULL || capture == NULL)
    {
      return -ENOMEM;
    }

  ret = audio_register("pcm0p", playback);
  if (ret < 0)
    {
      return ret;
    }

  ret = audio_register("pcm0c", capture);
  if (ret < 0)
    {
      audio_unregister("pcm0p", playback);
      return ret;
    }

  initialized = true;
#ifdef CONFIG_ESP32S31_I2S_DUPLEX
#ifdef CONFIG_ESP32S31_I2S_CAPTURE_16K_MONO
  syslog(LOG_INFO, "ES8311: pcm0c continuous native mono, 16000/16/1\n");
#elif defined(CONFIG_ESP32S31_I2S_DUPLEX_44K)
  syslog(LOG_INFO, "ES8311: pcm0p/pcm0c continuous, 44100/16/2 slots\n");
#else
  syslog(LOG_INFO, "ES8311: pcm0p/pcm0c duplex, 48000/16/2 slots\n");
#endif
#else
  syslog(LOG_INFO, "ES8311: raw PCM playback/capture, half-duplex\n");
#endif
  return OK;
}
