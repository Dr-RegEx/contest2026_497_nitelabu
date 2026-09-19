/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_reset.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdlib.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>

#include "espressif/esp_systemreset.h"

#ifdef CONFIG_BOARDCTL_RESET_CAUSE
#  include <errno.h>
#  include <sys/boardctl.h>
#  include "esp_rom_sys.h"
#endif

#ifdef CONFIG_BOARDCTL_RESET

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_reset
 *
 * Description:
 *   Run normal shutdown handlers before resetting both cores through the
 *   existing Espressif reset implementation.  Assertion resets must not
 *   depend on shutdown handlers making progress.
 ****************************************************************************/

int board_reset(int status)
{
  syslog(LOG_INFO, "reboot status=%d\n", status);

  if (status == EXIT_SUCCESS)
    {
      up_shutdown_handler();
    }

  up_systemreset();
  return 0;
}

#endif

#ifdef CONFIG_BOARDCTL_RESET_CAUSE
int board_reset_cause(struct boardioc_reset_cause_s *cause)
{
  soc_reset_reason_t reason;

  if (cause == NULL)
    {
      return -EINVAL;
    }

  reason = esp_rom_get_reset_reason(0);
  cause->flag = 0;
  switch (reason)
    {
      case RESET_REASON_CHIP_POWER_ON:
        cause->cause = BOARDIOC_RESETCAUSE_SYS_CHIPPOR;
        break;
      case RESET_REASON_SYS_RWDT:
        cause->cause = BOARDIOC_RESETCAUSE_SYS_RWDT;
        break;
      case RESET_REASON_CORE_RWDT:
        cause->cause = BOARDIOC_RESETCAUSE_CORE_RWDT;
        break;
      case RESET_REASON_CPU_RWDT:
        cause->cause = BOARDIOC_RESETCAUSE_CPU_RWDT;
        break;
      case RESET_REASON_CORE_MWDT0:
      case RESET_REASON_CORE_MWDT1:
        cause->cause = BOARDIOC_RESETCAUSE_CORE_MWDT;
        cause->flag = reason == RESET_REASON_CORE_MWDT1;
        break;
      case RESET_REASON_CPU_MWDT:
        cause->cause = BOARDIOC_RESETCAUSE_CPU_MWDT;
        break;
      case RESET_REASON_CORE_SW:
        cause->cause = BOARDIOC_RESETCAUSE_CORE_SOFT;
        break;
      case RESET_REASON_CPU_SW:
        cause->cause = BOARDIOC_RESETCAUSE_CPU_SOFT;
        break;
      case RESET_REASON_SYS_BROWN_OUT:
        cause->cause = BOARDIOC_RESETCAUSE_SYS_BOR;
        break;
      case RESET_REASON_CORE_DEEP_SLEEP:
        cause->cause = BOARDIOC_RESETCAUSE_CORE_DPSP;
        break;
      default:
        cause->cause = BOARDIOC_RESETCAUSE_UNKOWN;
        cause->flag = reason;
        break;
    }

  return OK;
}
#endif
