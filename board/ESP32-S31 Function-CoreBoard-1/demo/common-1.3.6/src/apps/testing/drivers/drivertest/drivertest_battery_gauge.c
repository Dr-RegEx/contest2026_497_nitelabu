/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_battery_gauge.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cmocka.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <nuttx/power/battery_ioctl.h>
#include <nuttx/power/battery_gauge.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BATTERY_DEFAULT_CHARGED 2

#define OPTARG_TO_VALUE(value, type, base)                            \
  do                                                                  \
    {                                                                 \
      FAR char *ptr;                                                  \
      value = (type)strtoul(optarg, &ptr, base);                      \
      if (*ptr != '\0')                                               \
        {                                                             \
          printf("Parameter error: -%c %s\n", ch, optarg);            \
          show_usage(argv[0], EXIT_FAILURE);                          \
        }                                                             \
    } while (0)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct battery_gauge_s
{
  char devpath[PATH_MAX];
  uint32_t present;
  uint32_t voltage;
  uint32_t capacity;
  uint32_t current;
  uint32_t temperature;
  uint32_t status;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: show_usage
 ****************************************************************************/

static void show_usage(FAR const char *progname, int exitcode)
{
  printf("Usage: %s"
         " -d <path>\n",
         progname);

  exit(exitcode);
}

/****************************************************************************
 * Name: parse_commandline
 ****************************************************************************/

static void parse_commandline(FAR struct battery_gauge_s *priv,
                              int argc, FAR char **argv)
{
  int ch;

  while ((ch = getopt(argc, argv, "d:")) != ERROR)
    {
      switch (ch)
        {
          case 'd':
            strlcpy(priv->devpath, optarg, sizeof(priv->devpath));
            break;

          case '?':
            printf("Unsupported option: %s\n", optarg);
            show_usage(argv[0], EXIT_FAILURE);
            break;
        }
    }
}

/****************************************************************************
 * Name: drivertest_battery_gauge
 ****************************************************************************/

static void drivertest_battery_gauge(FAR void **state)
{
  FAR struct battery_gauge_s *priv = *state;
  struct epoll_event ev;
  int epollfd;
  uint32_t mask;
  int value;
  int ret;
  int fd;
  int i;

  fd = open(priv->devpath, O_RDONLY);
  assert_true(fd > 0);

  /* Get Guage present */

  ret = ioctl(fd, BATIOC_ONLINE, &value);
  assert_return_code(ret, OK);
  assert_true((bool)value == true);

  /* Get Guage statue */

  ret = ioctl(fd, BATIOC_STATE, &value);
  assert_return_code(ret, OK);
  assert_in_range(value, 0, BATTERY_DISCHARGING);

  /* Get Guage voltage */

  ret = ioctl(fd, BATIOC_VOLTAGE, &value);
  assert_return_code(ret, OK);
  assert_in_range(value, 0, 5000);

  /* Get Guage current */

  ret = ioctl(fd, BATIOC_CURRENT, &value);
  assert_return_code(ret, OK);
  assert_true(value > -500);
  assert_true(value < 500);

  /* Get Guage capacity */

  ret = ioctl(fd, BATIOC_CAPACITY, &value);
  assert_return_code(ret, OK);
  assert_in_range(value, 0, 100);

  /* Get Guage temperature */

  ret = ioctl(fd, BATIOC_TEMPERATURE, &value);
  assert_return_code(ret, OK);
  assert_true(value > -400);
  assert_true(value < 500);

  epollfd = epoll_create1(EPOLL_CLOEXEC);
  assert_true(epollfd > 0);

  ev.events = POLLIN;
  ev.data.fd = fd;
  ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);

  for (i = 0; i < BATTERY_DEFAULT_CHARGED; i++)
    {
      /* epoll_wait */

      epoll_wait(epollfd, &ev, 1, -1);

      ret = read(fd, &mask, sizeof(uint32_t));
      assert_return_code(ret, OK);
      assert_int_not_equal(mask, 0);
    }

  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
  close(epollfd);
  close(fd);
}

int main(int argc, FAR char *argv[])
{
  struct battery_gauge_s gauge_data =
  {
    .devpath = "/dev/charger/gauge",
  };

  parse_commandline(&gauge_data, argc, argv);

  const struct CMUnitTest tests[] =
  {
    cmocka_unit_test_prestate(drivertest_battery_gauge, &gauge_data)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
