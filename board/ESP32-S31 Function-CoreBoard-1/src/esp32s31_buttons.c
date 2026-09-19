/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_buttons.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>

#include <nuttx/board.h>
#include <nuttx/irq.h>
#include <arch/irq.h>
#include <arch/board/board.h>

#include "espressif/esp_gpio.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

uint32_t board_button_initialize(void)
{
  /* The external BOOT switch grounds GPIO61.  Keep the strap input-only. */

  if (esp_configgpio(BOARD_BUTTON_BOOT_PIN, INPUT | PULLUP) < 0)
    {
      return 0;
    }

  return NUM_BUTTONS;
}

uint32_t board_buttons(void)
{
  return esp_gpioread(BOARD_BUTTON_BOOT_PIN) ? 0 : BUTTON_BOOT_BIT;
}

#ifdef CONFIG_ARCH_IRQBUTTONS
int board_button_irq(int id, xcpt_t handler, void *arg)
{
  int irq = ESP_PIN2IRQ(BOARD_BUTTON_BOOT_PIN);
  int ret;

  if (id != BUTTON_BOOT)
    {
      return -EINVAL;
    }

  esp_gpioirqdisable(irq);
  if (handler == NULL)
    {
      return irq_detach(irq);
    }

  ret = irq_attach(irq, handler, arg);
  if (ret < 0)
    {
      return ret;
    }

  esp_gpioirqenable(irq, CHANGE);
  return 0;
}
#endif
