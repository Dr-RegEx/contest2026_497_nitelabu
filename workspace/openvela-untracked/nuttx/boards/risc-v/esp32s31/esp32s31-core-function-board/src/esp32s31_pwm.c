/* SPDX-License-Identifier: Apache-2.0 */
#include <nuttx/config.h>
#include <nuttx/timers/pwm.h>
#include <syslog.h>
#include "esp32s31_pwm.h"

int esp32s31_pwm_setup(void)
{
  int ret = pwm_register("/dev/pwm0", esp32s31_pwm_initialize());
  if (ret == 0)
    {
      syslog(LOG_INFO, "xTS PWM: /dev/pwm0 GPIO48 J2.14 LEDC0 timer0 channel0\n");
    }

  return ret;
}
