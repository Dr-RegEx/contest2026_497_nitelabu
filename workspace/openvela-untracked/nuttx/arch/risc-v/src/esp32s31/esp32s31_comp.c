/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <nuttx/analog/comp.h>
#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/mutex.h>
#include <nuttx/spinlock.h>
#include <nuttx/wdog.h>
#include "espressif/esp_gpio.h"
#include "espressif/esp_irq.h"
#include "esp_private/esp_clk.h"
#include "hal/ana_cmpr_ll.h"
#include "soc/interrupts.h"
#include "esp32s31_comp.h"

#define HW ANALOG_CMPR_LL_GET_HW(0)
#define CROSS_MASK ANALOG_CMPR_LL_ANY_CROSS_INTR_MASK(0, 0)
#define ALL_MASK ANALOG_CMPR_LL_ALL_INTR_MASK(0)
#define COMP_IRQ ESP_SOURCE2IRQ(ETS_ZERO_DET_INTR_SOURCE)
#define SCAN_TICKS MAX(1, MSEC2TICK(10))

/* S31 requires a software/ETM scan trigger. This basic single-channel
 * frontend samples every 10 ms (rounded to scheduler ticks). Hardware
 * crossing interrupts deliver changes to the standard read/poll interface.
 * It is not a continuous high-speed zero-crossing detector.
 */

static mutex_t g_lock = NXMUTEX_INITIALIZER;
static struct wdog_s g_scan;
static const struct comp_callback_s *g_callback;
static int g_pin;
static int g_cpuint = -1;
static unsigned int g_reference;
static uint32_t g_clock;
static bool g_active;
static bool g_registered;

static int s31_comp_bind(struct comp_dev_s *dev,
                         const struct comp_callback_s *callback)
{
  irqstate_t flags = enter_critical_section();
  g_callback = callback;
  leave_critical_section(flags);
  return OK;
}

static void s31_comp_scan(wdparm_t arg)
{
  if (g_active)
    {
      analog_cmpr_ll_start_scan(HW);
      wd_start(&g_scan, SCAN_TICKS, s31_comp_scan, 0);
    }
}

static int s31_comp_irq(int irq, void *context, void *arg)
{
  struct comp_dev_s *dev = arg;
  uint32_t status = analog_cmpr_ll_get_intr_status(HW);
  analog_cmpr_ll_clear_intr(HW, status);
  if (g_active && (status & CROSS_MASK) != 0 && g_callback != NULL)
    {
      g_callback->au_notify(dev,
                            analog_cmpr_ll_get_compare_result(HW, 0));
    }

  return OK;
}

static void s31_comp_shutdown(struct comp_dev_s *dev)
{
  irqstate_t flags = enter_critical_section();
  g_active = false;
  wd_cancel(&g_scan);
  if (g_cpuint >= 0)
    {
      up_disable_irq(COMP_IRQ);
      analog_cmpr_ll_enable_intr(HW, ALL_MASK, false);
      analog_cmpr_ll_enable(HW, false);
      analog_cmpr_ll_clear_intr(HW, ALL_MASK);
      analog_cmpr_ll_enable_function_clock(0, false);
      analog_cmpr_ll_enable_bus_clock(0, false);
      irq_detach(COMP_IRQ);
      esp_teardown_irq(ETS_ZERO_DET_INTR_SOURCE, g_cpuint);
      g_cpuint = -1;
    }

  leave_critical_section(flags);
}

static int s31_comp_setup(struct comp_dev_s *dev)
{
  irqstate_t flags;
  int ret;

  /* Disable digital input/output and pulls, retaining the GPIO mux. */

  ret = esp_configgpio(g_pin, 0);
  if (ret < 0)
    {
      return ret;
    }

  g_cpuint = esp_setup_irq(ETS_ZERO_DET_INTR_SOURCE,
                          ESP_IRQ_PRIORITY_DEFAULT, ESP_IRQ_TRIGGER_LEVEL);
  if (g_cpuint < 0)
    {
      ret = g_cpuint;
      g_cpuint = -1;
      return ret;
    }

  ret = irq_attach(COMP_IRQ, s31_comp_irq, dev);
  if (ret < 0)
    {
      esp_teardown_irq(ETS_ZERO_DET_INTR_SOURCE, g_cpuint);
      g_cpuint = -1;
      return ret;
    }

  flags = enter_critical_section();
  analog_cmpr_ll_enable_bus_clock(0, true);
  analog_cmpr_ll_reset_register(0);
  analog_cmpr_ll_reset_core(0);
  analog_cmpr_ll_set_clk_src(0, ANA_CMPR_CLK_SRC_XTAL);
  analog_cmpr_ll_set_clk_div(0, 1);
  analog_cmpr_ll_enable_function_clock(0, true);
  analog_cmpr_ll_enable_intr(HW, ALL_MASK, false);
  analog_cmpr_ll_clear_intr(HW, ALL_MASK);
  analog_cmpr_ll_set_ref_source(HW, ANA_CMPR_REF_SRC_INTERNAL);
  analog_cmpr_ll_set_internal_ref_voltage(HW, g_reference);
  analog_cmpr_ll_set_ref_hys_level(HW, ANA_CMPR_REF_HYS_LEVEL0);
  analog_cmpr_ll_set_src_pad(HW, 0, g_pin - 37);
  analog_cmpr_ll_set_scan_mask(HW, 1);
  analog_cmpr_ll_set_scan_mode(HW, ANA_CMPR_SCAN_MODE_FULL);
  analog_cmpr_ll_set_resample_limit(HW, 1);
  analog_cmpr_ll_set_cross_debounce_cycle(HW, g_clock / 100000);
  analog_cmpr_ll_set_poll_period(HW, g_clock / 500000);
  analog_cmpr_ll_enable_capture_timer(HW, false);
  analog_cmpr_ll_enable(HW, true);
  up_udelay(100);
  analog_cmpr_ll_start_scan(HW);
  up_udelay(100);
  analog_cmpr_ll_clear_intr(HW, ALL_MASK);
  g_active = true;
  analog_cmpr_ll_enable_intr(HW, CROSS_MASK, true);
  up_enable_irq(COMP_IRQ);
  ret = wd_start(&g_scan, SCAN_TICKS, s31_comp_scan, 0);
  leave_critical_section(flags);
  if (ret < 0)
    {
      s31_comp_shutdown(dev);
    }

  return ret;
}

static int s31_comp_read(struct comp_dev_s *dev)
{
  irqstate_t flags = enter_critical_section();
  int ret = g_active ? analog_cmpr_ll_get_compare_result(HW, 0) : -ENODEV;
  leave_critical_section(flags);
  return ret;
}

static int s31_comp_ioctl(struct comp_dev_s *dev, int cmd, unsigned long arg)
{
  return -ENOTTY;
}

static const struct comp_ops_s g_ops =
{
  .ao_bind = s31_comp_bind,
  .ao_setup = s31_comp_setup,
  .ao_shutdown = s31_comp_shutdown,
  .ao_read = s31_comp_read,
  .ao_ioctl = s31_comp_ioctl,
};

static struct comp_dev_s g_comp = { .ad_ops = &g_ops };

int esp32s31_comp_initialize(int pin, unsigned int ref_tenths)
{
  int clock = esp_clk_xtal_freq();
  int ret;

  if (pin < 37 || pin > 40 || ref_tenths < 1 || ref_tenths > 7 ||
      clock < 1000000)
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (g_registered)
    {
      ret = pin == g_pin && ref_tenths == g_reference ? OK : -EBUSY;
    }
  else
    {
      g_pin = pin;
      g_reference = ref_tenths;
      g_clock = clock;
      ret = comp_register("/dev/comp0", &g_comp);
      g_registered = ret == OK;
    }

  nxmutex_unlock(&g_lock);
  return ret;
}
