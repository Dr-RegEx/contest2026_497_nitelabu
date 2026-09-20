/****************************************************************************
 * SPDX-License-Identifier: Apache-2.0
 *
 * Opt-in watchdog candidate delay. The locked clock HAL maintains the ROM
 * CPU-ticks-per-microsecond value whenever it programs the CPU frequency.
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <sys/types.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>

#include "esp_rom_sys.h"
#include "esp32s31_wdt.h"

void up_udelay(useconds_t microseconds)
{
  /* Keep each ROM call short, including for the original four-second busy
   * wait. This avoids a large cycles-per-us multiplication in the ROM.
   */

  while (microseconds > 1000)
    {
      esp_rom_delay_us(1000);
      microseconds -= 1000;
    }

  if (microseconds > 0)
    {
      esp_rom_delay_us(microseconds);
    }
}

void up_mdelay(unsigned int milliseconds)
{
  while (milliseconds-- > 0)
    {
      up_udelay(1000);
    }
}

int esp32s31_wdt_delay_check(void)
{
  clock_t start = clock_systime_ticks();
  unsigned long elapsed;

  up_udelay(500000);
  elapsed = TICK2MSEC(clock_systime_ticks() - start);
  syslog(LOG_INFO, "S31 WDT delay check: requested=500ms measured=%lums\n",
         elapsed);

  /* The original API test expects its 500 ms ping wait within 50 ms. This
   * is one bounded boot diagnostic, not an alternate watchdog test.
   */

  return elapsed >= 450 && elapsed <= 550 ? OK : -ERANGE;
}
