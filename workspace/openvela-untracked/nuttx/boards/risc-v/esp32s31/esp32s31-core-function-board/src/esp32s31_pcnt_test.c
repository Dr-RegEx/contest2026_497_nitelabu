/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>
#include <nuttx/arch.h>
#include <nuttx/sensors/qencoder.h>
#include "espressif/esp_gpio.h"
#include "soc/gpio_pins.h"
#include "soc/gpio_sig_map.h"

/* Each transition changes one quadrature phase. Change its copy used as
 * the other channel's direction control first, then its pulse input.
 * Only the matrix constants are routed: physical GPIO pads stay untouched.
 */

static void phase(bool a, bool high)
{
  int pin = high ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT;

  esp_gpio_matrix_in(pin, a ? PCNT0_CTRL_CH1_IN0_IDX : PCNT0_CTRL_CH0_IN0_IDX,
                     false);
  esp_gpio_matrix_in(pin, a ? PCNT0_SIG_CH0_IN0_IDX : PCNT0_SIG_CH1_IN0_IDX,
                     false);
  up_udelay(100);
}

static void cycles(bool forward, unsigned int n)
{
  while (n-- > 0)
    {
      phase(forward, true);
      phase(!forward, true);
      phase(forward, false);
      phase(!forward, false);
    }
}

static int position(int fd, const char *step, int32_t expected)
{
  int32_t actual = INT32_MIN;

  if (ioctl(fd, QEIOC_POSITION, (unsigned long)(uintptr_t)&actual) < 0)
    {
      return -errno;
    }

  syslog(LOG_INFO, "S31PCNT step=%s expected=%" PRId32 " actual=%" PRId32
         "\n", step, expected, actual);
  return actual == expected ? OK : -EIO;
}

int esp32s31_pcnt_test(void)
{
  int first = -1;
  int second = -1;
  int ret;

  first = open("/dev/qe0", O_RDONLY);
  if (first < 0)
    {
      return -errno;
    }

  ret = position(first, "initial", 0);
  if (ret < 0)
    {
      goto out;
    }

  cycles(true, 16);
  ret = position(first, "forward", 64);
  if (ret < 0)
    {
      goto out;
    }

  second = open("/dev/qe0", O_RDONLY);
  if (second < 0)
    {
      ret = -errno;
      goto out;
    }

  ret = position(second, "shared-open", 64);
  if (ret < 0)
    {
      goto out;
    }

  ret = close(first);
  first = -1;
  if (ret < 0)
    {
      ret = -errno;
      goto out;
    }

  cycles(false, 8);
  ret = position(second, "shared-close", 32);
  if (ret < 0)
    {
      goto out;
    }

  if (ioctl(second, QEIOC_RESET, 0) < 0)
    {
      ret = -errno;
      goto out;
    }

  ret = position(second, "reset", 0);
  if (ret < 0)
    {
      goto out;
    }

  cycles(false, 16);
  ret = position(second, "reverse", -64);
  if (ret < 0)
    {
      goto out;
    }

  ret = close(second);
  second = -1;
  if (ret < 0)
    {
      ret = -errno;
      goto out;
    }

  first = open("/dev/qe0", O_RDONLY);
  if (first < 0)
    {
      ret = -errno;
      goto out;
    }

  ret = position(first, "reopen", 0);
  if (ret < 0)
    {
      goto out;
    }

  cycles(true, 4);
  ret = position(first, "reopen-forward", 16);

out:
  if (first >= 0 && close(first) < 0 && ret == OK)
    {
      ret = -errno;
    }

  if (second >= 0 && close(second) < 0 && ret == OK)
    {
      ret = -errno;
    }

  if (ret == OK)
    {
      syslog(LOG_INFO, "S31PCNT PASS checks=8 source=internal-matrix\n");
    }

  return ret;
}
