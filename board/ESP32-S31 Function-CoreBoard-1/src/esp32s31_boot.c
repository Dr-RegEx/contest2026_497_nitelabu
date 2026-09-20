/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_boot.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <syslog.h>

#include "esp32s31-core-function-board.h"

void esp_board_initialize(void)
{
}

#ifdef CONFIG_BOARDCTL
int board_app_initialize(uintptr_t arg)
{
#ifdef CONFIG_BOARD_LATE_INITIALIZE
  return OK;
#else
  return esp_bringup();
#endif
}
#endif

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  int ret = esp_bringup();

  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: ESP32-S31 board bring-up failed: %d\n", ret);
    }
}
#endif
