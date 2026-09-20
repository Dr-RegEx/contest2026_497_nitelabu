/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_xts_gpio.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>

#include <arch/irq.h>
#include <nuttx/irq.h>
#include <nuttx/ioexpander/gpio.h>

#include "espressif/esp_gpio.h"
#include "soc/gpio_sig_map.h"

#ifdef CONFIG_ESP32S31_XTS_GPIO

/* Board schematic V1.0, sheet 2: GPIO47 and GPIO48 connect only to
 * J2 pins 13 and 14.  Neither is a strapping pin.  Connect these two
 * header pins with a jumper for the original GPIO loop/interrupt tests.
 * Both pads boot as inputs; the original test selects output explicitly.
 */

struct s31_xts_gpio_s
{
  struct gpio_dev_s gpio;
  pin_interrupt_t callback;
  uint8_t pin;
  gpio_intrtype_t intrtype;
  volatile unsigned int irq_count;
};

static int s31_gpio_interrupt(int irq, void *context, void *arg)
{
  struct s31_xts_gpio_s *priv = arg;

  priv->irq_count++;

  /* Latch a level event until the next GPIOC_REGISTER.  Leaving a held
   * level enabled would repeatedly enter the ISR and prevent the test
   * thread from running GPIOC_UNREGISTER.  Edge events remain enabled.
   */

  if (priv->intrtype == ONHIGH || priv->intrtype == ONLOW)
    {
      esp_gpioirqdisable(ESP_PIN2IRQ(priv->pin));
    }

  if (priv->callback != NULL)
    {
      return priv->callback(&priv->gpio, priv->pin);
    }

  return OK;
}

static int s31_gpio_read(struct gpio_dev_s *dev, bool *value)
{
  struct s31_xts_gpio_s *priv = (struct s31_xts_gpio_s *)dev;

  *value = esp_gpioread(priv->pin);
  return OK;
}

static int s31_gpio_write(struct gpio_dev_s *dev, bool value)
{
  struct s31_xts_gpio_s *priv = (struct s31_xts_gpio_s *)dev;

  esp_gpiowrite(priv->pin, value);
  return OK;
}

static int s31_gpio_attach(struct gpio_dev_s *dev,
                           pin_interrupt_t callback)
{
  struct s31_xts_gpio_s *priv = (struct s31_xts_gpio_s *)dev;
  int irq = ESP_PIN2IRQ(priv->pin);
  int ret;

  esp_gpioirqdisable(irq);
  priv->callback = NULL;
  if (callback == NULL)
    {
      irq_detach(irq);
      return OK;
    }

  ret = irq_attach(irq, s31_gpio_interrupt, priv);
  if (ret >= 0)
    {
      priv->callback = callback;
    }

  return ret;
}

static int s31_gpio_enable(struct gpio_dev_s *dev, bool enable)
{
  struct s31_xts_gpio_s *priv = (struct s31_xts_gpio_s *)dev;
  int irq = ESP_PIN2IRQ(priv->pin);

  if (!enable)
    {
      esp_gpioirqdisable(irq);
      syslog(LOG_INFO, "S31_GPIO_IRQ pin=%u type=%u count=%u\n",
             (unsigned int)priv->pin, (unsigned int)priv->intrtype,
             priv->irq_count);
    }
  else if (priv->callback != NULL && priv->intrtype != DISABLED)
    {
      priv->irq_count = 0;
      esp_gpioirqenable(irq, priv->intrtype);
    }
  else
    {
      return -EINVAL;
    }

  return OK;
}

static int s31_gpio_setpintype(struct gpio_dev_s *dev,
                               enum gpio_pintype_e pintype)
{
  struct s31_xts_gpio_s *priv = (struct s31_xts_gpio_s *)dev;
  gpio_intrtype_t intrtype = DISABLED;
  gpio_pinattr_t attr = INPUT;
  int ret;

  switch (pintype)
    {
      case GPIO_INPUT_PIN:
        break;
      case GPIO_INPUT_PIN_PULLUP:
        attr |= PULLUP;
        break;
      case GPIO_INPUT_PIN_PULLDOWN:
        attr |= PULLDOWN;
        break;
      case GPIO_OUTPUT_PIN:
        attr |= OUTPUT;
        break;
      case GPIO_OUTPUT_PIN_OPENDRAIN:
        attr |= OUTPUT_OPEN_DRAIN;
        break;
      case GPIO_INTERRUPT_PIN:
      case GPIO_INTERRUPT_RISING_PIN:
        intrtype = RISING;
        break;
      case GPIO_INTERRUPT_FALLING_PIN:
        intrtype = FALLING;
        break;
      case GPIO_INTERRUPT_BOTH_PIN:
        intrtype = CHANGE;
        break;
      case GPIO_INTERRUPT_HIGH_PIN:
        intrtype = ONHIGH;
        break;
      case GPIO_INTERRUPT_LOW_PIN:
        intrtype = ONLOW;
        break;
      default:
        return -ENOTSUP;
    }

  esp_gpioirqdisable(ESP_PIN2IRQ(priv->pin));
  esp_gpio_matrix_out(priv->pin, SIG_GPIO_OUT_IDX, false, false);
  ret = esp_configgpio(priv->pin, attr);
  if (ret >= 0)
    {
      priv->intrtype = intrtype;
      dev->gp_pintype = pintype;
    }

  return ret;
}

static const struct gpio_operations_s g_s31_gpio_ops =
{
  .go_read       = s31_gpio_read,
  .go_write      = s31_gpio_write,
  .go_attach     = s31_gpio_attach,
  .go_enable     = s31_gpio_enable,
  .go_setpintype = s31_gpio_setpintype,
};

static struct s31_xts_gpio_s g_s31_xts_gpio[2] =
{
  { .gpio = { .gp_ops = &g_s31_gpio_ops }, .pin = 47 },
  { .gpio = { .gp_ops = &g_s31_gpio_ops }, .pin = 48 }
};

int esp32s31_xts_gpio_initialize(void)
{
  int ret;
  int i;

  for (i = 0; i < 2; i++)
    {
      ret = s31_gpio_setpintype(&g_s31_xts_gpio[i].gpio, GPIO_INPUT_PIN);
      if (ret < 0)
        {
          return ret;
        }

      /* Expose the two pins as gpio0 and gpio1; the profile names them. */

      ret = gpio_pin_register(&g_s31_xts_gpio[i].gpio, i);
      if (ret < 0)
        {
          if (i > 0)
            {
              gpio_pin_unregister(&g_s31_xts_gpio[0].gpio, 0);
            }

          return ret;
        }
    }

  syslog(LOG_INFO, "S31_XTS_GPIO: /dev/gpio0=GPIO47 J2.13 "
         "/dev/gpio1=GPIO48 J2.14 (external jumper required)\n");
  return OK;
}

#endif /* CONFIG_ESP32S31_XTS_GPIO */
