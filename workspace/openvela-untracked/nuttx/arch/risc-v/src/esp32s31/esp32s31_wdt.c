/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_wdt.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <syslog.h>

#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <nuttx/timers/watchdog.h>

#include "esp_clk.h"
#include "esp_irq.h"
#include "hal/wdt_hal.h"
#include "hal/rwdt_ll.h"
#include "riscv/interrupt.h"
#include "soc/interrupts.h"
#include "soc/rtc.h"
#include "esp32s31_wdt.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define S31_WDT_IRQ       ESP_SOURCE2IRQ(ETS_LP_WDT_INTR_SOURCE)
#define S31_WDT_PRIORITY  7
#define S31_WDT_DUMP_MS   5000

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct s31_wdt_s
{
  struct watchdog_lowerhalf_s lower;
  void *upper;
  xcpt_t handler;
  uint32_t timeout;
  clock_t lastfeed;
  int cpuint;
  bool started;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static wdt_hal_context_t g_hal = RWDT_HAL_CONTEXT_DEFAULT();

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void s31_wdt_program(struct s31_wdt_s *priv)
{
  uint32_t frequency = rtc_clk_slow_freq_get_hz();

  wdt_hal_config_stage(&g_hal, WDT_STAGE0,
                      (uint64_t)priv->timeout * frequency / 1000,
                      WDT_STAGE_ACTION_INT);
  wdt_hal_config_stage(&g_hal, WDT_STAGE1,
                      (uint64_t)S31_WDT_DUMP_MS * frequency / 1000,
                      priv->handler ? WDT_STAGE_ACTION_OFF :
                                      WDT_STAGE_ACTION_RESET_RTC);
  wdt_hal_config_stage(&g_hal, WDT_STAGE2, 0, WDT_STAGE_ACTION_OFF);
  wdt_hal_config_stage(&g_hal, WDT_STAGE3, 0, WDT_STAGE_ACTION_OFF);
  wdt_hal_feed(&g_hal);
  priv->lastfeed = clock_systime_ticks();
}

static int s31_wdt_start(struct watchdog_lowerhalf_s *lower)
{
  struct s31_wdt_s *priv = (struct s31_wdt_s *)lower;
  irqstate_t flags = enter_critical_section();

  if (priv->started)
    {
      leave_critical_section(flags);
      return -EBUSY;
    }

  wdt_hal_write_protect_disable(&g_hal);
  s31_wdt_program(priv);
  wdt_hal_handle_intr(&g_hal);
  priv->started = true;
  up_enable_irq(S31_WDT_IRQ);
  wdt_hal_enable(&g_hal);
  wdt_hal_write_protect_enable(&g_hal);
  leave_critical_section(flags);
  return OK;
}

static int s31_wdt_stop(struct watchdog_lowerhalf_s *lower)
{
  struct s31_wdt_s *priv = (struct s31_wdt_s *)lower;
  irqstate_t flags = enter_critical_section();

  up_disable_irq(S31_WDT_IRQ);
  wdt_hal_write_protect_disable(&g_hal);
  wdt_hal_disable(&g_hal);
  wdt_hal_handle_intr(&g_hal);
  wdt_hal_write_protect_enable(&g_hal);
  priv->started = false;
  leave_critical_section(flags);
  return OK;
}

static int s31_wdt_keepalive(struct watchdog_lowerhalf_s *lower)
{
  struct s31_wdt_s *priv = (struct s31_wdt_s *)lower;
  irqstate_t flags = enter_critical_section();

  wdt_hal_write_protect_disable(&g_hal);
  wdt_hal_feed(&g_hal);
  wdt_hal_write_protect_enable(&g_hal);
  priv->lastfeed = clock_systime_ticks();
  leave_critical_section(flags);
  return OK;
}

static int s31_wdt_getstatus(struct watchdog_lowerhalf_s *lower,
                           struct watchdog_status_s *status)
{
  struct s31_wdt_s *priv = (struct s31_wdt_s *)lower;
  irqstate_t flags = enter_critical_section();
  uint32_t elapsed = TICK2MSEC(clock_systime_ticks() - priv->lastfeed);

  status->flags = priv->handler ? WDFLAGS_CAPTURE : WDFLAGS_RESET;
  if (priv->started)
    {
      status->flags |= WDFLAGS_ACTIVE;
    }

  status->timeout = priv->timeout;
  status->timeleft = elapsed >= priv->timeout ? 0 : priv->timeout - elapsed;
  leave_critical_section(flags);
  return OK;
}

static int s31_wdt_settimeout(struct watchdog_lowerhalf_s *lower,
                            uint32_t timeout)
{
  struct s31_wdt_s *priv = (struct s31_wdt_s *)lower;
  uint32_t frequency = rtc_clk_slow_freq_get_hz();
  uint64_t ticks = (uint64_t)timeout * frequency / 1000;
  irqstate_t flags;

  if (ticks < 16 || ticks > UINT32_MAX)
    {
      return -ERANGE;
    }

  flags = enter_critical_section();
  up_disable_irq(S31_WDT_IRQ);
  priv->timeout = timeout;
  wdt_hal_write_protect_disable(&g_hal);
  s31_wdt_program(priv);
  wdt_hal_write_protect_enable(&g_hal);
  if (priv->started)
    {
      up_enable_irq(S31_WDT_IRQ);
    }

  leave_critical_section(flags);
  return OK;
}

static xcpt_t s31_wdt_capture(struct watchdog_lowerhalf_s *lower,
                             xcpt_t handler)
{
  struct s31_wdt_s *priv = (struct s31_wdt_s *)lower;
  irqstate_t flags = enter_critical_section();
  xcpt_t previous = priv->handler;

  up_disable_irq(S31_WDT_IRQ);
  priv->handler = handler;
  esprv_int_set_priority(priv->cpuint, handler ? 1 : S31_WDT_PRIORITY);
  wdt_hal_write_protect_disable(&g_hal);
  s31_wdt_program(priv);
  wdt_hal_handle_intr(&g_hal);
  wdt_hal_write_protect_enable(&g_hal);
  if (priv->started)
    {
      up_enable_irq(S31_WDT_IRQ);
    }

  leave_critical_section(flags);
  return previous;
}

static int s31_wdt_interrupt(int irq, void *context, void *arg)
{
  struct s31_wdt_s *priv = arg;

  wdt_hal_write_protect_disable(&g_hal);
  /* HAL handle_intr also feeds the watchdog. Only acknowledge here so a
   * fatal timeout preserves stage 1's independent hardware reset deadline.
   */

  rwdt_ll_clear_intr_status(g_hal.rwdt_dev);
  if (priv->handler != NULL)
    {
      wdt_hal_feed(&g_hal);
      wdt_hal_write_protect_enable(&g_hal);
      priv->lastfeed = clock_systime_ticks();
      return priv->handler(irq, context, priv->upper);
    }

  /* Do not feed: stage 1 performs an actual RTC-system watchdog reset
   * after NuttX prints the interrupted task's panic/register/stack data.
   */

  wdt_hal_write_protect_enable(&g_hal);
  syslog(LOG_EMERG, "S31 RWDT timeout; hardware reset after panic dump\n");
  PANIC();
  for (; ; )
    {
    }
}

static const struct watchdog_ops_s g_ops =
{
  .start      = s31_wdt_start,
  .stop       = s31_wdt_stop,
  .keepalive  = s31_wdt_keepalive,
  .getstatus  = s31_wdt_getstatus,
  .settimeout = s31_wdt_settimeout,
  .capture    = s31_wdt_capture,
};

static struct s31_wdt_s g_wdt =
{
  .lower = { .ops = &g_ops },
  .timeout = 2000,
  .cpuint = -1,
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int esp32s31_wdt_initialize(const char *path)
{
  int ret;

#ifdef CONFIG_ESP32S31_WDT_ROM_DELAY
  ret = esp32s31_wdt_delay_check();
  if (ret < 0)
    {
      return ret;
    }
#endif


  g_wdt.cpuint = esp_setup_irq(ETS_LP_WDT_INTR_SOURCE, S31_WDT_PRIORITY,
                              ESP_IRQ_TRIGGER_LEVEL);
  if (g_wdt.cpuint < 0)
    {
      return g_wdt.cpuint;
    }

  wdt_hal_init(&g_hal, WDT_RWDT, 0, true);
  ret = irq_attach(S31_WDT_IRQ, s31_wdt_interrupt, &g_wdt);
  if (ret < 0)
    {
      esp_teardown_irq(ETS_LP_WDT_INTR_SOURCE, g_wdt.cpuint);
      return ret;
    }

  g_wdt.upper = watchdog_register(path, &g_wdt.lower);
  if (g_wdt.upper == NULL)
    {
      irq_detach(S31_WDT_IRQ);
      esp_teardown_irq(ETS_LP_WDT_INTR_SOURCE, g_wdt.cpuint);
      return -ENOMEM;
    }

  return OK;
}
