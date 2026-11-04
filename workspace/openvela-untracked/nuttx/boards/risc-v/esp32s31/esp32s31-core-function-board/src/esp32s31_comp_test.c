/* SPDX-License-Identifier: Apache-2.0 */
#include <nuttx/config.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>
#include "espressif/esp_gpio.h"

static int sample(int fd, const char *step, int expected)
{
  uint8_t value = 0xff;
  int ret = read(fd, &value, 1);
  if (ret != 1)
    {
      return ret < 0 ? -errno : -EIO;
    }

  syslog(LOG_INFO, "S31COMP sample=%s expected=%d actual=%u\n",
         step, expected, value);
  return value == expected ? OK : -EIO;
}

static void *change_pin(void *arg)
{
  usleep(30000);
  esp_gpiowrite(37, (uintptr_t)arg != 0);
  return NULL;
}

static int transition(int fd, const char *step, int level)
{
  struct pollfd pfd = { .fd = fd, .events = POLLIN };
  pthread_t thread;
  int ret = pthread_create(&thread, NULL, change_pin,
                           (void *)(uintptr_t)level);
  int saved;
  int joined;
  if (ret != 0)
    {
      return -ret;
    }

  ret = poll(&pfd, 1, 300);
  saved = errno;
  joined = pthread_join(thread, NULL);
  syslog(LOG_INFO, "S31COMP poll=%s ready=%d input=%d\n",
         step, ret, !!(pfd.revents & POLLIN));
  if (ret != 1 || pfd.revents != POLLIN || joined != 0)
    {
      return ret < 0 ? -saved : -EIO;
    }

  return sample(fd, step, level);
}

int esp32s31_comp_test(void)
{
  int first = -1;
  int second = -1;
  int ret;
  uint8_t sentinel = 0xa5;

  first = open("/dev/comp0", O_RDONLY | O_NONBLOCK);
  if (first < 0)
    {
      return -errno;
    }

  esp_gpiowrite(37, false);
  ret = esp_configgpio(37, OUTPUT);
  if (ret < 0)
    {
      goto out;
    }

  usleep(30000);
  ret = sample(first, "initial-low", 0);
  if (ret < 0)
    {
      goto out;
    }

  if (read(first, &sentinel, 0) != 0 || sentinel != 0xa5 ||
      ioctl(first, 0x7fffffff, 0) != -1 || errno != ENOTTY)
    {
      ret = -EIO;
      goto out;
    }

  syslog(LOG_INFO, "S31COMP contract zero-read=0 unknown-ioctl=ENOTTY\n");
  second = open("/dev/comp0", O_RDONLY | O_NONBLOCK);
  if (second < 0)
    {
      ret = -errno;
      goto out;
    }

  ret = transition(first, "shared-high", 1);
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

  ret = transition(second, "shared-close-low", 0);
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

  first = open("/dev/comp0", O_RDONLY | O_NONBLOCK);
  if (first < 0)
    {
      ret = -errno;
      goto out;
    }

  esp_gpiowrite(37, false);
  ret = esp_configgpio(37, OUTPUT);
  if (ret < 0)
    {
      goto out;
    }

  usleep(30000);
  ret = sample(first, "reopen-low", 0);
  if (ret == OK)
    {
      ret = transition(first, "reopen-high", 1);
    }

  if (ret == OK)
    {
      ret = transition(first, "reopen-final-low", 0);
    }

out:
  esp_gpiowrite(37, false);
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
      syslog(LOG_INFO, "S31COMP PASS samples=6 polls=4 pin=37 reference=50\n");
    }

  return ret;
}
