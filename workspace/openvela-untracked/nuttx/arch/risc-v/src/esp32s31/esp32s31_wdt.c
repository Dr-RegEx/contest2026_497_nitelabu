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
#include <nuttx/arch.h>
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

#ifdef CONFIG_ESP32S31_KORVO_HMI_NETDIAG
#  include "esp_cpu.h"
#  include "esp_flash_diag.h"
#  include "hal/systimer_ll.h"
#  include "soc/uart_struct.h"
#  include "soc/clic_reg.h"
#  include "soc/interrupt_core0_reg.h"
#endif

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
#ifdef CONFIG_ESP32S31_KORVO_HMI_NETDIAG
  /* PANIC saves the assertion site's registers. Capture the interrupted
   * context first, and read timer state without requesting a snapshot:
   * the normal counter helper may itself be the stalled operation.
   */

  syslog(LOG_EMERG, "NETDIAG interrupted context; timer conf=%08lx "
         "unit0=%08lx unit1=%08lx\n",
         (unsigned long)SYSTIMER.conf.val,
         (unsigned long)SYSTIMER.unit_op[0].val,
         (unsigned long)SYSTIMER.unit_op[1].val);
  up_dump_register(context);
  esp32s31_timer_stall_dump();
  for (unsigned bank = 0; bank < (ESP_NSOURCES + 31) / 32; bank++)
    {
      uint32_t routed_to_tick = 0;
      uint32_t tick_route = REG_READ(DR_REG_INTERRUPT_CORE0_BASE +
                                    4 * ETS_SYSTIMER_TARGET0_INTR_SOURCE);
      for (unsigned bit = 0; bit < 32 && bank * 32 + bit < ESP_NSOURCES;
           bit++)
        {
          uint32_t route = REG_READ(DR_REG_INTERRUPT_CORE0_BASE +
                                    4 * (bank * 32 + bit));
          if ((route & 0x3f) == (tick_route & 0x3f))
            {
              routed_to_tick |= 1u << bit;
            }
        }

      syslog(LOG_EMERG, "NETDIAG route bank=%u tick_shared=%08lx "
             "tick_route=%08lx clic=%08lx\n", bank,
             (unsigned long)routed_to_tick, (unsigned long)tick_route,
             (unsigned long)REG_READ(CLIC_INT_CTRL_REG(tick_route & 0x3f)));
    }
  syslog(LOG_EMERG, "NETDIAG UART raw=%08lx status=%08lx enable=%08lx "
         "fifo=%08lx cpu_unmask=%08lx\n",
         (unsigned long)UART0.int_raw.val,
         (unsigned long)UART0.int_st.val,
         (unsigned long)UART0.int_ena.val,
         (unsigned long)UART0.status.val,
         (unsigned long)esp_cpu_intr_get_enabled_mask());
#endif
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
