/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_pwm.c
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <nuttx/timers/pwm.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp32s31_pwm.h"
#include "espressif/esp_gpio.h"
#include "esp_private/esp_clk.h"
#include "hal/ledc_ll.h"
#include "soc/gpio_sig_map.h"

#if defined(CONFIG_PWM_MULTICHAN) || defined(CONFIG_PWM_PULSECOUNT)
#  error "S31 PWM currently supports one continuous output"
#endif

#define S31_PWM_PIN 48
#define S31_PWM_HW LEDC_LL_GET_HW(0)
#define S31_PWM_MODE LEDC_LOW_SPEED_MODE
#define S31_PWM_TIMER LEDC_TIMER_0
#define S31_PWM_CHANNEL LEDC_CHANNEL_0

struct s31_pwm_s
{
  struct pwm_lowerhalf_s lower;
  bool ready;
};

static int s31_pwm_stop(struct pwm_lowerhalf_s *lower)
{
  struct s31_pwm_s *priv = (struct s31_pwm_s *)lower;
  irqstate_t flags = enter_critical_section();
  if (priv->ready)
    {
      ledc_ll_set_idle_level(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL, 0);
      ledc_ll_set_sig_out_en(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL, false);
      ledc_ll_ls_channel_update(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL);
      ledc_ll_timer_pause(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER);
      ledc_ll_ls_timer_update(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER);
    }

  leave_critical_section(flags);
  return 0;
}

static int s31_pwm_setup(struct pwm_lowerhalf_s *lower)
{
  struct s31_pwm_s *priv = (struct s31_pwm_s *)lower;
  irqstate_t flags;
  int ret;
  int __DECLARE_RCC_ATOMIC_ENV = 0;

  ret = esp_configgpio(S31_PWM_PIN, OUTPUT);
  if (ret < 0)
    {
      return ret;
    }

  esp_gpiowrite(S31_PWM_PIN, false);
  flags = enter_critical_section();
  ledc_ll_enable_bus_clock(0, true);
  ledc_ll_reset_register(0);
  ledc_ll_enable_clock(0, true);
  ledc_ll_mem_power_by_pmu(S31_PWM_HW);
  ledc_ll_mem_set_low_power_mode(S31_PWM_HW, LEDC_LL_MEM_LP_MODE_SHUT_DOWN);
  ledc_ll_enable_timer_power(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER, true);
  ledc_ll_enable_channel_power(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL, true);
  ledc_ll_set_slow_clk_sel(S31_PWM_HW, LEDC_SLOW_CLK_XTAL);
  ledc_ll_enable_interrupt(S31_PWM_HW, UINT32_MAX, false);
  ledc_ll_clear_intr_status(S31_PWM_HW, UINT32_MAX);
  ledc_ll_bind_channel_timer(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL,
                            S31_PWM_TIMER);
  ledc_ll_set_idle_level(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL, 0);
  ledc_ll_set_sig_out_en(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL, false);
  ledc_ll_ls_channel_update(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL);
  ledc_ll_timer_pause(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER);
  ledc_ll_ls_timer_update(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER);
  priv->ready = true;
  leave_critical_section(flags);
  esp_gpio_matrix_out(S31_PWM_PIN, LEDC0_LS_SIG_OUT_PAD_OUT0_IDX, false, false);
  return 0;
}

static int s31_pwm_shutdown(struct pwm_lowerhalf_s *lower)
{
  struct s31_pwm_s *priv = (struct s31_pwm_s *)lower;
  irqstate_t flags;
  int __DECLARE_RCC_ATOMIC_ENV = 0;

  s31_pwm_stop(lower);
  flags = enter_critical_section();
  if (priv->ready)
    {
      ledc_ll_enable_channel_power(S31_PWM_HW, S31_PWM_MODE,
                                   S31_PWM_CHANNEL, false);
      ledc_ll_enable_timer_power(S31_PWM_HW, S31_PWM_MODE,
                                 S31_PWM_TIMER, false);
      ledc_ll_enable_clock(0, false);
      ledc_ll_enable_bus_clock(0, false);
      priv->ready = false;
    }

  leave_critical_section(flags);
  esp_gpio_matrix_out(S31_PWM_PIN, SIG_GPIO_OUT_IDX, false, false);
  return esp_configgpio(S31_PWM_PIN, INPUT);
}

static int s31_pwm_start(struct pwm_lowerhalf_s *lower,
                         const struct pwm_info_s *info)
{
  struct s31_pwm_s *priv = (struct s31_pwm_s *)lower;
  uint64_t denominator;
  uint64_t divider = 0;
  uint32_t duty;
  int resolution;
  int xtal = esp_clk_xtal_freq();
  irqstate_t flags;
  unsigned int range;

  if (info == NULL || info->frequency == 0 || info->duty > b16ONE || xtal <= 0)
    {
      return -EINVAL;
    }

  /* Pick the highest bounded resolution with an 18-bit 8.8 divider.
   * Fourteen bits also leaves room for exactly 100 percent duty.
   */

  for (resolution = 14; resolution >= 1; resolution--)
    {
      denominator = (uint64_t)info->frequency << resolution;
      divider = (((uint64_t)xtal << LEDC_LL_FRACTIONAL_BITS) +
                 denominator / 2) / denominator;
      if (divider >= 256 && divider <= 0x3ffff)
        {
          break;
        }
    }

  if (resolution < 1)
    {
      return -ERANGE;
    }

  duty = ((uint64_t)info->duty * (1u << resolution) + 32768) >> 16;
  flags = enter_critical_section();
  if (!priv->ready)
    {
      leave_critical_section(flags);
      return -EIO;
    }

  ledc_ll_set_sig_out_en(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL, false);
  ledc_ll_ls_channel_update(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL);
  ledc_ll_timer_pause(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER);
  ledc_ll_set_clock_divider(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER, divider);
  ledc_ll_set_duty_resolution(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER, resolution);
  ledc_ll_ls_timer_update(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER);
  ledc_ll_timer_rst(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER);
  ledc_ll_set_hpoint(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL, 0);
  ledc_ll_set_duty_int_part(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL, duty);

  /* The S31 gamma RAM is not reset with the peripheral registers. */

  for (range = 0; range < SOC_LEDC_GAMMA_CURVE_FADE_RANGE_MAX; range++)
    {
      ledc_ll_set_fade_param_range(S31_PWM_HW, S31_PWM_MODE,
                                   S31_PWM_CHANNEL, range, 0, 0, 0, 0);
    }

  ledc_ll_set_fade_param_range(S31_PWM_HW, S31_PWM_MODE,
                               S31_PWM_CHANNEL, 0, 1, 1, 0, 1);
  ledc_ll_set_range_number(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL, 1);
  ledc_ll_timer_resume(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER);
  ledc_ll_ls_timer_update(S31_PWM_HW, S31_PWM_MODE, S31_PWM_TIMER);
  ledc_ll_set_sig_out_en(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL, true);
  ledc_ll_set_duty_start(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL);
  ledc_ll_ls_channel_update(S31_PWM_HW, S31_PWM_MODE, S31_PWM_CHANNEL);
  leave_critical_section(flags);
  return 0;
}

static int s31_pwm_ioctl(struct pwm_lowerhalf_s *lower, int cmd,
                         unsigned long arg)
{
  return -ENOTTY;
}

static const struct pwm_ops_s g_s31_pwm_ops =
{
  .setup = s31_pwm_setup,
  .shutdown = s31_pwm_shutdown,
  .start = s31_pwm_start,
  .stop = s31_pwm_stop,
  .ioctl = s31_pwm_ioctl
};

static struct s31_pwm_s g_s31_pwm =
{
  .lower = { .ops = &g_s31_pwm_ops }
};

struct pwm_lowerhalf_s *esp32s31_pwm_initialize(void)
{
  return &g_s31_pwm.lower;
}
