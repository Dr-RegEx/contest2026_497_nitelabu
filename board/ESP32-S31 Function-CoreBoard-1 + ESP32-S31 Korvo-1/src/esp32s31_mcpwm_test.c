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
#include <nuttx/timers/pwm.h>
#include <nuttx/timers/capture.h>
#include "espressif/esp_gpio.h"

static int output(int fd, uint32_t hz, unsigned int percent)
{
  struct pwm_info_s info = {0};
  info.frequency = hz;
  info.duty = (uint64_t)b16ONE * percent / 100;
  if (ioctl(fd, PWMIOC_SETCHARACTERISTICS,
            (unsigned long)(uintptr_t)&info) < 0 ||
      ioctl(fd, PWMIOC_START, 0) < 0)
    {
      return -errno;
    }

  return OK;
}

static int measure(int fd, const char *step, uint32_t hz, unsigned int duty)
{
  struct cap_all_s all = {0};
  usleep(100000);
  if (ioctl(fd, CAPIOC_ALL, (unsigned long)(uintptr_t)&all) < 0)
    {
      return -errno;
    }

  syslog(LOG_INFO, "S31MCPWM measure=%s target=%" PRIu32 "/%u actual=%"
         PRIu32 "/%u edges=%" PRIu32 "\n",
         step, hz, duty, all.freq, all.duty, all.edges);
  return all.freq >= hz - hz / 50 - 1 && all.freq <= hz + hz / 50 + 1 &&
         all.duty >= duty - 2 && all.duty <= duty + 2 && all.edges >= 8 ?
         OK : -EIO;
}

static int quiet(int fd, const char *step)
{
  uint32_t before;
  uint32_t after;
  int level;
  usleep(20000);
  if (ioctl(fd, CAPIOC_EDGES, (unsigned long)(uintptr_t)&before) < 0)
    {
      return -errno;
    }

  usleep(50000);
  if (ioctl(fd, CAPIOC_EDGES, (unsigned long)(uintptr_t)&after) < 0)
    {
      return -errno;
    }

  level = esp_gpioread(37);
  syslog(LOG_INFO, "S31MCPWM quiet=%s delta=%" PRIu32 " level=%d\n",
         step, after - before, level);
  return before == after && level == 0 ? OK : -EIO;
}

static int level(int pwm, unsigned int percent)
{
  int ret = output(pwm, 1000, percent);
  int actual;
  if (ret < 0)
    {
      return ret;
    }

  usleep(10000);
  actual = esp_gpioread(37);
  syslog(LOG_INFO, "S31MCPWM level=%u actual=%d\n", percent, actual);
  return actual == (percent != 0) ? OK : -EIO;
}

int esp32s31_mcpwm_test(void)
{
  int cap = -1;
  int pwm = -1;
  int ret;

  cap = open("/dev/cap0", O_RDONLY);
  if (cap < 0)
    {
      return -errno;
    }

  pwm = open("/dev/mcpwm0", O_RDONLY);
  if (pwm < 0)
    {
      ret = -errno;
      goto out;
    }

  ret = output(pwm, 1000, 25);
  if (ret < 0 || (ret = measure(cap, "first", 1000, 25)) < 0)
    {
      goto out;
    }

  ret = output(pwm, 2000, 75);
  if (ret < 0 || (ret = measure(cap, "change-high", 2000, 75)) < 0)
    {
      goto out;
    }

  ret = output(pwm, 500, 50);
  if (ret < 0 || (ret = measure(cap, "change-low", 500, 50)) < 0)
    {
      goto out;
    }

  if (ioctl(pwm, PWMIOC_STOP, 0) < 0)
    {
      ret = -errno;
      goto out;
    }

  ret = quiet(cap, "stop");
  if (ret < 0 || (ret = output(pwm, 500, 50)) < 0 ||
      (ret = measure(cap, "restart", 500, 50)) < 0)
    {
      goto out;
    }

  ret = close(cap);
  cap = -1;
  if (ret < 0)
    {
      ret = -errno;
      goto out;
    }

  cap = open("/dev/cap0", O_RDONLY);
  if (cap < 0)
    {
      ret = -errno;
      goto out;
    }

  ret = measure(cap, "capture-reopen", 500, 50);
  if (ret < 0)
    {
      goto out;
    }

  ret = close(pwm);
  pwm = -1;
  if (ret < 0)
    {
      ret = -errno;
      goto out;
    }

  ret = quiet(cap, "pwm-close");
  if (ret < 0)
    {
      goto out;
    }

  pwm = open("/dev/mcpwm0", O_RDONLY);
  if (pwm < 0)
    {
      ret = -errno;
      goto out;
    }

  ret = output(pwm, 1000, 50);
  if (ret < 0 || (ret = measure(cap, "pwm-reopen", 1000, 50)) < 0 ||
      (ret = level(pwm, 100)) < 0 || (ret = level(pwm, 0)) < 0)
    {
      goto out;
    }

out:
  if (pwm >= 0 && close(pwm) < 0 && ret == OK)
    {
      ret = -errno;
    }

  if (cap >= 0 && close(cap) < 0 && ret == OK)
    {
      ret = -errno;
    }

  if (ret == OK)
    {
      syslog(LOG_INFO, "S31MCPWM PASS measures=6 quiet=2 levels=2 pin=37\n");
    }

  return ret;
}
