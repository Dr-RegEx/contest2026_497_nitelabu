/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <nuttx/mutex.h>
#include <nuttx/spinlock.h>
#include <nuttx/sensors/qencoder.h>

#include "espressif/esp_gpio.h"
#include "hal/pcnt_ll.h"
#include "soc/gpio_pins.h"
#include "soc/gpio_sig_map.h"
#include "soc/soc_caps.h"
#include "esp32s31_qencoder.h"

/* This first S31 lower half owns PCNT0 exclusively and exposes unit 0.
 * The standard upper half serializes first-open, last-close and ioctls.
 * Register access is additionally protected against local interruption.
 * Other units/groups and counter overflow extension are not exposed.
 */

static mutex_t g_registration_lock = NXMUTEX_INITIALIZER;
static bool g_registered;
static bool g_active;
static int g_pin_a;
static int g_pin_b;

static void s31_qe_route(int a, int b)
{
  esp_gpio_matrix_in(a, PCNT0_SIG_CH0_IN0_IDX, false);
  esp_gpio_matrix_in(b, PCNT0_CTRL_CH0_IN0_IDX, false);
  esp_gpio_matrix_in(b, PCNT0_SIG_CH1_IN0_IDX, false);
  esp_gpio_matrix_in(a, PCNT0_CTRL_CH1_IN0_IDX, false);
  esp_gpio_matrix_in(GPIO_MATRIX_CONST_ZERO_INPUT, PCNT0_RST_IN0_IDX,
                     false);
}

static int s31_qe_setup(struct qe_lowerhalf_s *lower)
{
  pcnt_dev_t *hw = PCNT_LL_GET_HW(0);
  irqstate_t flags;
  int ret;

  if (g_pin_a >= 0)
    {
      ret = esp_configgpio(g_pin_a, INPUT_FUNCTION | PULLUP);
      if (ret < 0)
        {
          return ret;
        }

      ret = esp_configgpio(g_pin_b, INPUT_FUNCTION | PULLUP);
      if (ret < 0)
        {
          return ret;
        }
    }

  flags = enter_critical_section();
  pcnt_ll_enable_bus_clock(0, true);
  pcnt_ll_reset_register(0);
  pcnt_ll_set_clock_source(hw, PCNT_CLK_SRC_APB);
  pcnt_ll_stop_count(hw, 0);
  pcnt_ll_enable_intr(hw, 1u, false); /* S31 takes a unit mask. */
  pcnt_ll_disable_all_events(hw, 0);
  pcnt_ll_enable_step_notify(hw, 0, false);
  pcnt_ll_set_high_limit_value(hw, 0, INT16_MAX);
  pcnt_ll_set_low_limit_value(hw, 0, INT16_MIN);
  pcnt_ll_set_glitch_filter_thres(hw, 0,
                                CONFIG_ESP32S31_PCNT_FILTER_CYCLES);
  pcnt_ll_enable_glitch_filter(hw, 0,
                              CONFIG_ESP32S31_PCNT_FILTER_CYCLES != 0);
  pcnt_ll_set_edge_action(hw, 0, 0, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                  PCNT_CHANNEL_EDGE_ACTION_INCREASE);
  pcnt_ll_set_edge_action(hw, 0, 1, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                  PCNT_CHANNEL_EDGE_ACTION_DECREASE);
  pcnt_ll_set_level_action(hw, 0, 0, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                   PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
  pcnt_ll_set_level_action(hw, 0, 1, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                   PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
  s31_qe_route(g_pin_a < 0 ? GPIO_MATRIX_CONST_ZERO_INPUT : g_pin_a,
               g_pin_b < 0 ? GPIO_MATRIX_CONST_ZERO_INPUT : g_pin_b);
  pcnt_ll_clear_count(hw, 0);
  pcnt_ll_clear_intr_status(hw, 1u);
  pcnt_ll_start_count(hw, 0);
  g_active = true;
  leave_critical_section(flags);
  return OK;
}

static int s31_qe_shutdown(struct qe_lowerhalf_s *lower)
{
  irqstate_t flags = enter_critical_section();
  pcnt_dev_t *hw = PCNT_LL_GET_HW(0);

  pcnt_ll_stop_count(hw, 0);
  pcnt_ll_enable_intr(hw, 1u, false);
  pcnt_ll_disable_all_events(hw, 0);
  pcnt_ll_clear_intr_status(hw, 1u);
  s31_qe_route(GPIO_MATRIX_CONST_ZERO_INPUT, GPIO_MATRIX_CONST_ZERO_INPUT);
  pcnt_ll_enable_bus_clock(0, false);
  g_active = false;
  leave_critical_section(flags);
  return OK;
}

static int s31_qe_position(struct qe_lowerhalf_s *lower, int32_t *pos)
{
  irqstate_t flags;
  int ret = OK;

  if (pos == NULL)
    {
      return -EINVAL;
    }

  flags = enter_critical_section();
  if (!g_active)
    {
      ret = -ENODEV;
    }
  else
    {
      *pos = pcnt_ll_get_count(PCNT_LL_GET_HW(0), 0);
    }

  leave_critical_section(flags);
  return ret;
}

static int s31_qe_reset(struct qe_lowerhalf_s *lower)
{
  irqstate_t flags = enter_critical_section();
  int ret = OK;

  if (!g_active)
    {
      ret = -ENODEV;
    }
  else
    {
      pcnt_ll_clear_count(PCNT_LL_GET_HW(0), 0);
      pcnt_ll_clear_intr_status(PCNT_LL_GET_HW(0), 1u);
    }

  leave_critical_section(flags);
  return ret;
}

static int s31_qe_unsupported(struct qe_lowerhalf_s *lower, uint32_t pos)
{
  return -ENOTTY;
}

static int s31_qe_ioctl(struct qe_lowerhalf_s *lower, int cmd,
                        unsigned long arg)
{
  return -ENOTTY;
}

static const struct qe_ops_s g_qe_ops =
{
  .setup     = s31_qe_setup,
  .shutdown  = s31_qe_shutdown,
  .position  = s31_qe_position,
  .setposmax = s31_qe_unsupported,
  .reset     = s31_qe_reset,
  .setindex  = s31_qe_unsupported,
  .ioctl     = s31_qe_ioctl,
};

static struct qe_lowerhalf_s g_qe_lower = { .ops = &g_qe_ops };

int esp32s31_qencoder_initialize(const char *path, int pin_a, int pin_b)
{
  int ret;

  if (path == NULL || path[0] != '/' ||
      !((pin_a == -1 && pin_b == -1) ||
        (pin_a >= 0 && pin_a < SOC_GPIO_PIN_COUNT &&
         pin_b >= 0 && pin_b < SOC_GPIO_PIN_COUNT && pin_a != pin_b &&
         (SOC_GPIO_VALID_GPIO_MASK & (1ull << pin_a)) != 0 &&
         (SOC_GPIO_VALID_GPIO_MASK & (1ull << pin_b)) != 0)))
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&g_registration_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (g_registered)
    {
      ret = -EBUSY;
    }
  else
    {
      g_pin_a = pin_a;
      g_pin_b = pin_b;
      ret = qe_register(path, &g_qe_lower);
      g_registered = ret == OK;
    }

  nxmutex_unlock(&g_registration_lock);
  return ret;
}
