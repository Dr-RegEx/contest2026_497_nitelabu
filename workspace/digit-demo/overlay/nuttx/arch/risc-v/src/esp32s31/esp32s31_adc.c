/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_adc.c
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/mutex.h>

#include "espressif/esp_gpio.h"
#include "esp_private/regi2c_ctrl.h"
#include "hal/adc_ll.h"
#include "soc/adc_channel.h"

/* Isolated board fixture: ADC1 channel5, GPIO47, J2 pin13. The pinned
 * S31 LL exposes a 17-bit result field. Preserve it as raw ADC data;
 * the reference port does not yet supply a calibrated voltage scheme.
 */

#define S31_ADC_CHANNEL ADC1_GPIO47_CHANNEL
#define S31_ADC_PIN     47
#define S31_ADC_TIMEOUT_US 10000

struct s31_adc_s
{
  const struct adc_callback_s *callback;
  mutex_t lock;
  bool ready;
  volatile bool receive;
};

static struct s31_adc_s g_adc =
{
  .lock = NXMUTEX_INITIALIZER
};

static int s31_adc_bind(struct adc_dev_s *dev,
                        const struct adc_callback_s *callback)
{
  struct s31_adc_s *priv = dev->ad_priv;
  priv->callback = callback;
  return OK;
}

static void s31_adc_reset(struct adc_dev_s *dev)
{
  struct s31_adc_s *priv = dev->ad_priv;

  /* Registration must not power up or start conversions. */

  if (priv->ready)
    {
      adc_oneshot_ll_disable_channel(ADC_UNIT_1);
      adc_oneshot_ll_clear_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE);
    }
}

static int s31_adc_setup(struct adc_dev_s *dev)
{
  struct s31_adc_s *priv = dev->ad_priv;
  irqstate_t flags;
  int __DECLARE_RCC_ATOMIC_ENV = 0;
  int ret = nxmutex_lock(&priv->lock);

  if (ret < 0)
    {
      return ret;
    }

  /* Match the reference gpio_config_as_analog: floating pad, digital
   * input/output disabled, no pull resistors, GPIO mux function.
   */

  ret = esp_configgpio(S31_ADC_PIN, 0);
  if (ret == OK && !priv->ready)
    {
      flags = enter_critical_section();
      adc_ll_enable_bus_clock(true);
      adc_ll_reset_register();
      adc_ll_enable_func_clock(true);
      ANALOG_CLOCK_ENABLE();
      regi2c_ctrl_ll_i2c_sar_periph_enable();
      adc_ll_digi_clk_sel(ADC_DIGI_CLK_SRC_XTAL);
      adc_ll_digi_controller_clk_div(ADC_LL_CLKM_DIV_NUM_DEFAULT,
                                     ADC_LL_CLKM_DIV_B_DEFAULT,
                                     ADC_LL_CLKM_DIV_A_DEFAULT);
      ADC.int_ena.val = 0;
      adc_ll_set_power_manage(ADC_UNIT_1, ADC_LL_POWER_SW_ON);
      adc_oneshot_ll_disable_all_unit();
      adc_oneshot_ll_clear_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE);
      priv->ready = true;
      leave_critical_section(flags);
      up_udelay(100);
    }

  nxmutex_unlock(&priv->lock);
  return ret;
}

static void s31_adc_shutdown(struct adc_dev_s *dev)
{
  struct s31_adc_s *priv = dev->ad_priv;
  irqstate_t flags;
  int __DECLARE_RCC_ATOMIC_ENV = 0;

  if (nxmutex_lock(&priv->lock) < 0)
    {
      return;
    }

  if (priv->ready)
    {
      flags = enter_critical_section();
      adc_oneshot_ll_disable_all_unit();
      adc_ll_set_power_manage(ADC_UNIT_1, ADC_LL_POWER_SW_OFF);
      regi2c_ctrl_ll_i2c_sar_periph_disable();
      ANALOG_CLOCK_DISABLE();
      adc_ll_enable_func_clock(false);
      adc_ll_enable_bus_clock(false);
      priv->ready = false;
      priv->receive = false;
      leave_critical_section(flags);
    }

  nxmutex_unlock(&priv->lock);
}

static void s31_adc_rxint(struct adc_dev_s *dev, bool enable)
{
  struct s31_adc_s *priv = dev->ad_priv;
  priv->receive = enable;
}

static int s31_adc_ioctl(struct adc_dev_s *dev, int cmd, unsigned long arg)
{
  struct s31_adc_s *priv = dev->ad_priv;
  uint32_t raw = 0;
  int elapsed;
  int ret;

  if (cmd == ANIOC_GET_NCHANNELS)
    {
      return 1;
    }

  if (cmd != ANIOC_TRIGGER)
    {
      return -ENOTTY;
    }

  ret = nxmutex_lock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  if (!priv->ready || !priv->receive || priv->callback == NULL)
    {
      nxmutex_unlock(&priv->lock);
      return -EIO;
    }

  adc_oneshot_ll_set_output_bits(ADC_UNIT_1, ADC_LL_RTC_MAX_BITWIDTH);
  adc_oneshot_ll_set_channel(ADC_UNIT_1, S31_ADC_CHANNEL);
  adc_oneshot_ll_clear_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE);
  adc_oneshot_ll_enable(ADC_UNIT_1);
  adc_oneshot_ll_start(ADC_UNIT_1);
  ret = -ETIMEDOUT;

  for (elapsed = 0; elapsed < S31_ADC_TIMEOUT_US; elapsed++)
    {
      if (adc_oneshot_ll_get_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE))
        {
          raw = adc_oneshot_ll_get_raw_result(ADC_UNIT_1);
          ret = OK;
          break;
        }

      up_udelay(1);
    }

  adc_oneshot_ll_disable_channel(ADC_UNIT_1);
  adc_oneshot_ll_clear_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE);
  if (ret == OK)
    {
      ret = priv->callback->au_receive(dev, S31_ADC_CHANNEL, raw);
    }

  nxmutex_unlock(&priv->lock);
  return ret;
}

static const struct adc_ops_s g_adc_ops =
{
  .ao_bind = s31_adc_bind,
  .ao_reset = s31_adc_reset,
  .ao_setup = s31_adc_setup,
  .ao_shutdown = s31_adc_shutdown,
  .ao_rxint = s31_adc_rxint,
  .ao_ioctl = s31_adc_ioctl
};

static struct adc_dev_s g_adc_dev =
{
  .ad_ops = &g_adc_ops,
  .ad_priv = &g_adc
};

int esp32s31_adc_setup(void)
{
  return adc_register("/dev/adc0", &g_adc_dev);
}
