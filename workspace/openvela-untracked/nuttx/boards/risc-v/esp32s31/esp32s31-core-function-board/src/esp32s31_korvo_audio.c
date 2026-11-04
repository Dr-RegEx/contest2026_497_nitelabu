/****************************************************************************
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#include <errno.h>
#include <syslog.h>
#include "espressif/esp_gpio.h"
#include "espressif/esp_i2c.h"
#include "esp32s31_i2s.h"
#include "esp32s31_pcm.h"
#include "esp32s31_board_resources.h"

#define KORVO_PA_PIN 7

static mutex_t g_init_lock = NXMUTEX_INITIALIZER;
static bool g_registered[2];
static bool g_bus_ready;

static void korvo_pa(bool enabled)
{
  esp_gpiowrite(KORVO_PA_PIN, enabled);
}

static struct s31_es8389_s g_codec =
{
  .lock = NXMUTEX_INITIALIZER,
  .pa = korvo_pa,
  .volume = 500,
};

/* The transport may force PA off, but only the codec may enable it, after
 * clocks are running and DAC unmute succeeds. DMA START alone is not enough.
 */

void esp32s31_audio_pa(bool enabled)
{
  if (!enabled)
    {
      korvo_pa(false);
    }
}

int esp32s31_korvo_audio_initialize(void)
{
  struct i2s_dev_s *playback;
  struct i2s_dev_s *capture;
  int ret;

  nxmutex_lock(&g_init_lock);
  if (g_registered[0] && g_registered[1])
    {
      ret = OK;
      goto out;
    }

  if (!g_bus_ready)
    {
      korvo_pa(false);
      ret = esp_configgpio(KORVO_PA_PIN, OUTPUT);
      if (ret < 0)
        {
          goto out;
        }

      /* Keep one board-lifetime reference, including on registration retry.
       * Camera SCCB and touch share this bus through the I2C master's lock.
       */

      if (g_codec.i2c == NULL)
        {
          g_codec.i2c = esp_i2cbus_initialize(0);
        }

      if (g_codec.i2c == NULL)
        {
          ret = -ENODEV;
          goto out;
        }

      playback = esp32s31_i2s_duplex_initialize(false);
      capture = esp32s31_i2s_duplex_initialize(true);
      ret = s31_pcm_initialize(&g_codec, playback, capture);
      if (ret < 0)
        {
          goto out;
        }

      g_bus_ready = true;
    }

  if (!g_registered[0])
    {
      ret = audio_register("pcm0p", s31_pcm_endpoint(false));
      if (ret < 0)
        {
          goto out;
        }

      g_registered[0] = true;
    }

  if (!g_registered[1])
    {
      ret = audio_register("pcm0c", s31_pcm_endpoint(true));
      if (ret < 0)
        {
          /* Keep a successfully exposed playback device intact. Retrying
           * registration must not reset a device already used by a client.
           */

          goto out;
        }

      g_registered[1] = true;
    }

  syslog(LOG_INFO, "Korvo ES8389: pcm0p/pcm0c, 16000/16/2, no MCLK output\n");
  ret = OK;
out:
  nxmutex_unlock(&g_init_lock);
  return ret;
}
