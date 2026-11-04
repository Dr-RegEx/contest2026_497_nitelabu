/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_temperature.c
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/mutex.h>
#include <nuttx/spinlock.h>
#include <nuttx/sensors/sensor.h>

#include "esp_private/regi2c_ctrl.h"
#include "hal/temperature_sensor_hal.h"
#include "hal/temperature_sensor_ll.h"
#include "esp32s31_temperature.h"

/* The board keeps this lower half registered for its lifetime. The sensor
 * upper half activates it for the first reader and stops it after the last
 * reader closes. Direct fetch uses O_NONBLOCK, as specified by sensor.h.
 * The current S31 configuration excludes other analog/RF clients until their
 * shared analog clock and power ownership is qualified together.
 */

static mutex_t g_temp_lock = NXMUTEX_INITIALIZER;
static bool g_registered;
static bool g_active;
static int g_trim;

static void s31_temp_power(bool enable)
{
  irqstate_t flags;
  int __DECLARE_RCC_ATOMIC_ENV = 0;

  flags = enter_critical_section();
  if (enable)
    {
      ANALOG_CLOCK_ENABLE();
      regi2c_ctrl_ll_i2c_sar_periph_enable();
      temperature_sensor_ll_bus_clk_enable(true);
      temperature_sensor_ll_reset_module();
      temperature_sensor_ll_enable_intr(false);
      temperature_sensor_ll_wakeup_enable(false);
      temperature_sensor_ll_clk_sel(TEMPERATURE_SENSOR_CLK_SRC_DEFAULT);
      temperature_sensor_hal_init();
      temperature_sensor_ll_enable(true);
      temperature_sensor_ll_sample_enable(true);
    }
  else
    {
      temperature_sensor_ll_sample_enable(false);
      temperature_sensor_ll_enable(false);
      temperature_sensor_ll_bus_clk_enable(false);
      regi2c_ctrl_ll_i2c_sar_periph_disable();
      ANALOG_CLOCK_DISABLE();
    }

  leave_critical_section(flags);
  if (enable)
    {
      /* The pinned S31 reference requires settling after power/range changes.
       * Keep the delay outside the interrupt-disabled register sequence.
       */

      up_udelay(300);
    }
}

static int s31_temp_activate(struct sensor_lowerhalf_s *lower,
                             struct file *filep, bool enable)
{
  int ret = nxmutex_lock(&g_temp_lock);

  if (ret < 0)
    {
      return ret;
    }

  if (!g_registered)
    {
      ret = -ENODEV;
    }
  else if (g_active != enable)
    {
      s31_temp_power(enable);
      g_active = enable;
    }

  nxmutex_unlock(&g_temp_lock);
  return ret;
}

static int s31_temp_fetch(struct sensor_lowerhalf_s *lower,
                          struct file *filep, char *buffer, size_t buflen)
{
  struct sensor_temp event = {0};
  int ret;

  if (buffer == NULL || buflen < sizeof(event))
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&g_temp_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (!g_active)
    {
      ret = -EAGAIN;
      goto out;
    }

  /* The HAL selects the appropriate DAC range and waits after range changes.
   * Its result is in whole degrees; the eFuse correction is in tenths.
   * A zero trim uses the nominal conversion, without claiming calibration.
   */

  event.temperature = temperature_sensor_hal_get_degree(NULL) -
                      (float)g_trim / 10.0f;
  if (event.temperature < TEMPERATURE_SENSOR_LL_MEASURE_MIN ||
      event.temperature > TEMPERATURE_SENSOR_LL_MEASURE_MAX)
    {
      ret = -ERANGE;
      goto out;
    }

  event.timestamp = sensor_get_timestamp();
  memcpy(buffer, &event, sizeof(event));
  ret = sizeof(event);

out:
  nxmutex_unlock(&g_temp_lock);
  return ret;
}

static const struct sensor_ops_s g_temp_ops =
{
  .activate = s31_temp_activate,
  .fetch = s31_temp_fetch
};

static struct sensor_lowerhalf_s g_temp_lower =
{
  /* This is die temperature. Keep the generic type's existing ABI spelling. */

  .type = SENSOR_TYPE_TEMPERAUTRE,
  .ops = &g_temp_ops
};

int esp32s31_temperature_initialize(void)
{
  int ret = nxmutex_lock(&g_temp_lock);

  if (ret < 0)
    {
      return ret;
    }

  if (!g_registered)
    {
      /* Registration does not power up the peripheral. A failed registration
       * can be retried without leaving clocks or a sampling worker behind.
       */

      g_trim = temperature_sensor_ll_load_calib_param();
      ret = sensor_register(&g_temp_lower, CONFIG_ESPRESSIF_TEMP_PATH_DEVNO);
      if (ret == OK)
        {
          g_registered = true;
          if (g_trim == 0)
            {
              syslog(LOG_WARNING,
                     "S31 temperature: zero eFuse trim, nominal conversion\n");
            }
        }
    }

  nxmutex_unlock(&g_temp_lock);
  return ret;
}
