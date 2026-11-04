/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef CONFIG_ESP32S31_MCPWM_TRACE
#  include <inttypes.h>
#  include <syslog.h>
#endif
#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/mutex.h>
#include <nuttx/spinlock.h>
#include <nuttx/timers/pwm.h>
#include <nuttx/timers/capture.h>
#include "espressif/esp_gpio.h"
#include "espressif/esp_irq.h"
#include "esp_private/esp_clk.h"
#include "hal/mcpwm_ll.h"
#include "soc/gpio_pins.h"
#include "soc/gpio_sig_map.h"
#include "soc/interrupts.h"
#include "soc/soc_caps.h"
#include "esp32s31_mcpwm.h"

#define HW MCPWM_LL_GET_HW(0)
#define CAP_EVENT MCPWM_LL_EVENT_CAPTURE(0)
#define CAP_IRQ ESP_SOURCE2IRQ(ETS_PWM0_INTR_SOURCE)

/* PWM and capture share a clock, but have independent first/last-open
 * lifetimes. A group reset must never disturb the other active client.
 * This driver is currently restricted to single-core FLAT configurations.
 */

static mutex_t g_lock = NXMUTEX_INITIALIZER;
static int g_pwm_pin;
static int g_cap_pin;
static int g_cpuint = -1;
static unsigned int g_refs;
static uint32_t g_clock;
static bool g_pwm_ready;
static bool g_timer_running;
static bool g_cap_ready;
static bool g_pwm_registered;
static bool g_cap_registered;
static uint32_t g_rise;
static uint32_t g_fall;
static uint32_t g_freq;
static uint32_t g_edges;
static uint8_t g_duty;
static bool g_have_rise;
static bool g_have_fall;
static bool g_valid;

static void s31_measure_reset(void)
{
  g_rise = g_fall = g_freq = g_edges = 0;
  g_duty = 0;
  g_have_rise = g_have_fall = g_valid = false;
}

static void s31_acquire(void)
{
  if (g_refs++ == 0)
    {
      mcpwm_ll_enable_bus_clock(0, true);
      mcpwm_ll_reset_register(0);
      mcpwm_ll_group_set_clock_source(0, SOC_MOD_CLK_XTAL);
      mcpwm_ll_group_set_clock_prescale(0, 1);
      mcpwm_ll_group_enable_clock(0, true);
      mcpwm_ll_group_enable_shadow_mode(HW);
      mcpwm_ll_intr_enable(HW, UINT32_MAX, false);
      mcpwm_ll_intr_clear_status(HW, UINT32_MAX);
    }
}

static void s31_release(void)
{
  if (--g_refs == 0)
    {
      mcpwm_ll_group_enable_clock(0, false);
      mcpwm_ll_enable_bus_clock(0, false);
    }
}

static int s31_pwm_stop(struct pwm_lowerhalf_s *lower)
{
  irqstate_t flags = enter_critical_section();
  int ret = OK;
  unsigned int wait;

  if (g_pwm_ready)
    {
      mcpwm_ll_gen_set_continue_force_level(HW, 0, 0, 0);
      if (g_timer_running)
        {
          /* Freezing the count does not complete the timer stop command.
           * Force a zero synchronization point, then wait for hardware's
           * stopped event before changing the prescaler. Keep the output
           * forced low throughout. The bound covers 256 XTAL ticks without
           * waiting for a complete low-frequency PWM period.
           */

          mcpwm_ll_intr_clear_status(HW, MCPWM_LL_EVENT_TIMER_STOP(0));
          mcpwm_ll_timer_set_start_stop_command(HW, 0,
                                               MCPWM_TIMER_STOP_EMPTY);
          mcpwm_ll_timer_set_sync_phase_value(HW, 0, 0);
          mcpwm_ll_timer_trigger_soft_sync(HW, 0);
          mcpwm_ll_timer_set_count_mode(HW, 0, MCPWM_TIMER_COUNT_MODE_UP);
          for (wait = 0; wait < 100; wait++)
            {
              if (HW->int_raw.val & MCPWM_LL_EVENT_TIMER_STOP(0))
                {
                  break;
                }

              up_udelay(1);
            }

          ret = wait == 100 ? -ETIMEDOUT : OK;
          if (ret == OK)
            {
              g_timer_running = false;
            }

          mcpwm_ll_intr_clear_status(HW, MCPWM_LL_EVENT_TIMER_STOP(0));
        }

      mcpwm_ll_timer_set_count_mode(HW, 0, MCPWM_TIMER_COUNT_MODE_PAUSE);
    }

  leave_critical_section(flags);
  return ret;
}

static int s31_pwm_setup(struct pwm_lowerhalf_s *lower)
{
  irqstate_t flags;
  int ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  esp_gpiowrite(g_pwm_pin, false);
  ret = esp_configgpio(g_pwm_pin, INPUT | OUTPUT);
  if (ret == OK)
    {
      flags = enter_critical_section();
      s31_acquire();
      mcpwm_ll_timer_set_count_mode(HW, 0, MCPWM_TIMER_COUNT_MODE_PAUSE);
      mcpwm_ll_gen_set_continue_force_level(HW, 0, 0, 0);
      mcpwm_ll_gen_disable_noncontinue_force_action(HW, 0, 0);
      mcpwm_ll_operator_connect_timer(HW, 0, 0);
      mcpwm_ll_generator_reset_actions(HW, 0, 0);
      g_pwm_ready = true;
      esp_gpio_matrix_out(g_pwm_pin, PWM0_CH0_A_PAD_OUT_IDX, false, false);
      leave_critical_section(flags);
    }

  nxmutex_unlock(&g_lock);
  return ret;
}

static int s31_pwm_shutdown(struct pwm_lowerhalf_s *lower)
{
  irqstate_t flags;
  int pinret;
  int ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  flags = enter_critical_section();
  if (g_pwm_ready)
    {
      ret = s31_pwm_stop(lower);
      esp_gpiowrite(g_pwm_pin, false);
      esp_gpio_matrix_out(g_pwm_pin, SIG_GPIO_OUT_IDX, false, false);
      /* Keep this board-reserved output at a defined inactive level after
       * disconnecting the peripheral. A weak pull-down does not guarantee
       * a low level on the board's loaded output pin.
       */

      pinret = esp_configgpio(g_pwm_pin, INPUT | OUTPUT);
      if (ret == OK)
        {
          ret = pinret;
        }

      g_pwm_ready = false;
      g_timer_running = false;
      s31_release();
    }

  leave_critical_section(flags);
  nxmutex_unlock(&g_lock);
  return ret;
}

static int s31_pwm_start(struct pwm_lowerhalf_s *lower,
                         const struct pwm_info_s *info)
{
  uint64_t period = 0;
  uint64_t denominator;
  uint32_t compare;
  unsigned int divider;
  irqstate_t flags;
  int ret;

  if (info == NULL || info->frequency == 0 || info->duty > b16ONE ||
      info->cpol != 0 || info->dcpol != 0)
    {
      return -EINVAL;
    }

  for (divider = 1; divider <= 256; divider++)
    {
      denominator = (uint64_t)divider * info->frequency;
      period = (g_clock + denominator / 2) / denominator;
      if (period <= 65536)
        {
          break;
        }
    }

  if (divider > 256 || period < 2)
    {
      return -ERANGE;
    }

  compare = (period * info->duty + 32768) >> 16;
  flags = enter_critical_section();
  if (!g_pwm_ready)
    {
      leave_critical_section(flags);
      return -ENODEV;
    }

  ret = s31_pwm_stop(lower);
  if (ret < 0)
    {
      leave_critical_section(flags);
      return ret;
    }

  mcpwm_ll_timer_set_clock_prescale(HW, 0, divider);
  mcpwm_ll_timer_set_peak(HW, 0, period, false);
  mcpwm_ll_timer_update_period_at_once(HW, 0);
  mcpwm_ll_operator_update_compare_at_once(HW, 0, 0);
  mcpwm_ll_operator_set_compare_value(HW, 0, 0, compare);
  mcpwm_ll_generator_set_action_on_timer_event(HW, 0, 0,
      MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY,
      MCPWM_GEN_ACTION_HIGH);
  mcpwm_ll_generator_set_action_on_compare_event(HW, 0, 0,
      MCPWM_TIMER_DIRECTION_UP, 0, MCPWM_GEN_ACTION_LOW);
  mcpwm_ll_timer_set_sync_phase_value(HW, 0, 0);
  mcpwm_ll_timer_set_sync_phase_direction(HW, 0, MCPWM_TIMER_DIRECTION_UP);
  mcpwm_ll_timer_enable_sync_input(HW, 0, true);
  mcpwm_ll_timer_trigger_soft_sync(HW, 0);
  mcpwm_ll_group_flush_shadow(HW);
  if (compare == 0 || compare == period)
    {
      mcpwm_ll_gen_set_continue_force_level(HW, 0, 0, compare != 0);
    }
  else
    {
      mcpwm_ll_gen_disable_continue_force_action(HW, 0, 0);
    }

  mcpwm_ll_timer_set_count_mode(HW, 0, MCPWM_TIMER_COUNT_MODE_UP);
  mcpwm_ll_timer_set_start_stop_command(HW, 0, MCPWM_TIMER_START_NO_STOP);
  g_timer_running = true;
#ifdef CONFIG_ESP32S31_MCPWM_TRACE
  uint32_t cfg0 = HW->timer[0].timern_cfg0.val;
  uint32_t cfg1 = HW->timer[0].timern_cfg1.val;
  uint32_t clockcfg = HP_SYS_CLKRST.mcpwm0_ctrl0.val;
#endif
  leave_critical_section(flags);
#ifdef CONFIG_ESP32S31_MCPWM_TRACE
  syslog(LOG_INFO, "S31MCPWM config frequency=%" PRIu32
         " divider=%u period=%" PRIu32 " cfg0=%08" PRIx32
         " cfg1=%08" PRIx32 " clock=%08" PRIx32 "\n",
         info->frequency, divider, (uint32_t)period, cfg0, cfg1, clockcfg);
#endif
  return OK;
}

static int s31_pwm_ioctl(struct pwm_lowerhalf_s *lower, int cmd,
                         unsigned long arg)
{
  return -ENOTTY;
}

static int s31_capture_irq(int irq, void *context, void *arg)
{
  uint32_t status = mcpwm_ll_intr_get_status(HW) & CAP_EVENT;
  uint32_t value;
  uint32_t period;
  uint32_t high;
  mcpwm_capture_edge_t edge;

  if (status == 0)
    {
      return OK;
    }

  value = mcpwm_ll_capture_get_value(HW, 0);
  edge = mcpwm_ll_capture_get_edge(HW, 0);
  mcpwm_ll_intr_clear_status(HW, status);
  g_edges++;
  if (edge == MCPWM_CAP_EDGE_POS)
    {
      period = value - g_rise;
      high = g_fall - g_rise;
      if (g_have_rise && g_have_fall && period > 0 && high < period)
        {
          g_freq = g_clock / period;
          g_duty = ((uint64_t)high * 100 + period / 2) / period;
          g_valid = true;
        }

      g_rise = value;
      g_have_rise = true;
      g_have_fall = false;
    }
  else if (g_have_rise)
    {
      g_fall = value;
      g_have_fall = true;
    }

  return OK;
}

static int s31_capture_start(struct cap_lowerhalf_s *lower)
{
  irqstate_t flags;
  int ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  g_cpuint = esp_setup_irq(ETS_PWM0_INTR_SOURCE, ESP_IRQ_PRIORITY_DEFAULT,
                           ESP_IRQ_TRIGGER_LEVEL);
  if (g_cpuint < 0)
    {
      ret = g_cpuint;
      goto out;
    }

  ret = irq_attach(CAP_IRQ, s31_capture_irq, NULL);
  if (ret < 0)
    {
      goto release_irq;
    }

  if (!(g_pwm_ready && g_cap_pin == g_pwm_pin))
    {
      ret = esp_configgpio(g_cap_pin, INPUT | PULLDOWN);
      if (ret < 0)
        {
          irq_detach(CAP_IRQ);
          goto release_irq;
        }
    }

  flags = enter_critical_section();
  s31_acquire();
  s31_measure_reset();
  esp_gpio_matrix_in(g_cap_pin, PWM0_CAP0_PAD_IN_IDX, false);
  mcpwm_ll_capture_enable_timer_sync(HW, false);
  mcpwm_ll_capture_enable_timer(HW, true);
  mcpwm_ll_capture_set_prescale(HW, 0, 1);
  mcpwm_ll_capture_enable_posedge(HW, 0, true);
  mcpwm_ll_capture_enable_negedge(HW, 0, true);
  mcpwm_ll_capture_enable_channel(HW, 0, true);
  mcpwm_ll_intr_clear_status(HW, CAP_EVENT);
  mcpwm_ll_intr_enable(HW, CAP_EVENT, true);
  g_cap_ready = true;
  up_enable_irq(CAP_IRQ);
  leave_critical_section(flags);
  goto out;

release_irq:
  esp_teardown_irq(ETS_PWM0_INTR_SOURCE, g_cpuint);
  g_cpuint = -1;
out:
  nxmutex_unlock(&g_lock);
  return ret;
}

static int s31_capture_stop(struct cap_lowerhalf_s *lower)
{
  irqstate_t flags;
  int ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  flags = enter_critical_section();
  if (g_cap_ready)
    {
      up_disable_irq(CAP_IRQ);
      mcpwm_ll_intr_enable(HW, CAP_EVENT, false);
      mcpwm_ll_capture_enable_channel(HW, 0, false);
      mcpwm_ll_capture_enable_timer(HW, false);
      mcpwm_ll_intr_clear_status(HW, CAP_EVENT);
      esp_gpio_matrix_in(GPIO_MATRIX_CONST_ZERO_INPUT,
                         PWM0_CAP0_PAD_IN_IDX, false);
      g_cap_ready = false;
      s31_measure_reset();
      s31_release();
    }

  leave_critical_section(flags);
  if (g_cpuint >= 0)
    {
      irq_detach(CAP_IRQ);
      esp_teardown_irq(ETS_PWM0_INTR_SOURCE, g_cpuint);
      g_cpuint = -1;
    }

  nxmutex_unlock(&g_lock);
  return OK;
}

static int s31_capture_getfreq(struct cap_lowerhalf_s *lower, uint32_t *value)
{
  irqstate_t flags = enter_critical_section();
  int ret = g_valid ? OK : -EAGAIN;
  if (ret == OK)
    {
      *value = g_freq;
    }

  leave_critical_section(flags);
  return ret;
}

static int s31_capture_getduty(struct cap_lowerhalf_s *lower, uint8_t *value)
{
  irqstate_t flags = enter_critical_section();
  int ret = g_valid ? OK : -EAGAIN;
  if (ret == OK)
    {
      *value = g_duty;
    }

  leave_critical_section(flags);
  return ret;
}

static int s31_capture_getedges(struct cap_lowerhalf_s *lower, uint32_t *value)
{
  irqstate_t flags = enter_critical_section();
  *value = g_edges;
  leave_critical_section(flags);
  return OK;
}

static const struct pwm_ops_s g_pwm_ops =
{
  .setup = s31_pwm_setup,
  .shutdown = s31_pwm_shutdown,
  .start = s31_pwm_start,
  .stop = s31_pwm_stop,
  .ioctl = s31_pwm_ioctl,
};

static const struct cap_ops_s g_cap_ops =
{
  .start = s31_capture_start,
  .stop = s31_capture_stop,
  .getfreq = s31_capture_getfreq,
  .getduty = s31_capture_getduty,
  .getedges = s31_capture_getedges,
};

static struct pwm_lowerhalf_s g_pwm = { .ops = &g_pwm_ops };
static struct cap_lowerhalf_s g_cap = { .ops = &g_cap_ops };

int esp32s31_mcpwm_initialize(int pwm_pin, int capture_pin)
{
  int clock = esp_clk_xtal_freq();
  int ret;

  if (pwm_pin < 0 || pwm_pin >= SOC_GPIO_PIN_COUNT ||
      capture_pin < 0 || capture_pin >= SOC_GPIO_PIN_COUNT ||
      !(SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ull << pwm_pin)) ||
      !(SOC_GPIO_VALID_GPIO_MASK & (1ull << capture_pin)) || clock <= 0)
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  if ((g_pwm_registered || g_cap_registered) &&
      (pwm_pin != g_pwm_pin || capture_pin != g_cap_pin))
    {
      ret = -EBUSY;
      goto out;
    }

  g_pwm_pin = pwm_pin;
  g_cap_pin = capture_pin;
  g_clock = clock;
  if (!g_pwm_registered)
    {
      ret = pwm_register("/dev/mcpwm0", &g_pwm);
      if (ret < 0)
        {
          goto out;
        }

      g_pwm_registered = true;
    }

  if (!g_cap_registered)
    {
      ret = cap_register("/dev/cap0", &g_cap);
      if (ret < 0)
        {
          goto out;
        }

      g_cap_registered = true;
    }

out:
  nxmutex_unlock(&g_lock);
  return ret;
}
