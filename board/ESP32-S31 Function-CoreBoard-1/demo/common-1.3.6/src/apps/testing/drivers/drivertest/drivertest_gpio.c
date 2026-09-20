/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_gpio.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <pthread.h>
#include <cmocka.h>

#include <nuttx/ioexpander/gpio.h>

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct gpio_test_s
{
  FAR const char *gpio_input;
  FAR const char *gpio_output;
  char irq_type;
  bool gpio_val;
  bool loop;
};

/****************************************************************************
 * Private Function
 ****************************************************************************/

/****************************************************************************
 * Name: show_usage
 ****************************************************************************/

static void show_usage(FAR const char *progname, int exitcode)
{
  printf("Usage: %s -i /dev/gpio0 -o dev/gpio1"
         "-p [value] -l [loop test]\n", progname);
  printf("Where:\n");
  printf("  -i <gpio_input> gpio_input location [eg: /dev/gpio0].\n");
  printf("  -o <gpio_output> gpio_output location [eg: /dev/gpio1].\n");
  printf("  -p <gpio_output> gpio_output value [eg: 1 or 0].\n");
  printf("  -r interrupt type[0:fall; 1:rise; 2:high; 3:low]\n");
  printf("  -l <loop test> [default: Test the input and output of GPIO ]\n");
  exit(exitcode);
}

/****************************************************************************
 * Name: parse_commandline
 ****************************************************************************/

static void parse_commandline(int argc, FAR char **argv,
                              FAR struct gpio_test_s *gpio)
{
  int option;

  while ((option = getopt(argc, argv, "i:o:p:r:l")) != ERROR)
    {
      switch (option)
        {
          case 'i':
            gpio->gpio_input = optarg;
            break;
          case 'o':
            gpio->gpio_output = optarg;
            break;
          case 'l':
            gpio->loop = true;
            break;
          case 'p':
            gpio->gpio_val = atoi(optarg);
            break;
          case 'r':
            gpio->irq_type = atoi(optarg);
            break;
          case '?':
            printf("Unknown option: %c\n", optopt);
            show_usage(argv[0], EXIT_FAILURE);
            break;
        }
    }
}

/****************************************************************************
 * Name: gpio_test_randbin
 ****************************************************************************/

static inline bool gpio_test_randbin(void)
{
  int value = rand() % 256;
  if (value > 127)
    {
      return true;
    }
  else
    {
      return false;
    }

  return true;
}

/****************************************************************************
 * Name: setup
 ****************************************************************************/

static int setup(FAR void **state)
{
  FAR struct gpio_test_s *gpio;
  time_t t;

  gpio = (FAR struct gpio_test_s *)*state;

  /* Seed the random number generated */

  srand((unsigned)time(&t));
  *state = gpio;
  return 0;
}

/****************************************************************************
 * Name: teardown
 ****************************************************************************/

static int teardown(FAR void **state)
{
  return 0;
}

/****************************************************************************
 * Name: drivertest_gpio_bool
 ****************************************************************************/

static void drivertest_gpio_bool(FAR void **state)
{
  FAR struct gpio_test_s *gpio = *state;
  bool outvalue;
  int fd;
  int ret;

  fd = open(gpio->gpio_output, O_RDWR);
  assert_false(fd < 0);

  /* Test Single GPIO I/O functionality */

  ret = ioctl(fd, GPIOC_SETPINTYPE, GPIO_OUTPUT_PIN);
  assert_false(ret < 0);

  /* write gpio val */

  ret = ioctl(fd, GPIOC_WRITE, gpio->gpio_val);
  assert_false(ret < 0);

  /* read gpio val */

  ret = ioctl(fd, GPIOC_READ, &outvalue);
  assert_false(ret < 0);

  printf("[write and read output]  writevalue is %d, outvalue is %d\n",
         gpio->gpio_val, outvalue);
  assert_int_equal(gpio->gpio_val, outvalue);

  close(fd);
}

/****************************************************************************
 * Name: drivertest_gpio_loop
 ****************************************************************************/

static void drivertest_gpio_loop(FAR void **state)
{
  FAR struct gpio_test_s *gpio = *state;
  int fd_in;
  int fd_out;
  bool invalue;
  int ret;

  if (gpio->loop == false)
    {
      return;
    }

  fd_in = open(gpio->gpio_input, O_RDWR);
  assert_false(fd_in < 0);
  fd_out = open(gpio->gpio_output, O_RDWR);
  assert_false(fd_out < 0);

  ret = ioctl(fd_out, GPIOC_SETPINTYPE, GPIO_OUTPUT_PIN);
  assert_false(ret < 0);

  ret = ioctl(fd_in, GPIOC_SETPINTYPE, GPIO_INPUT_PIN);
  assert_false(ret < 0);

  /* Set GPIO level */

  ret = ioctl(fd_out, GPIOC_WRITE, gpio->gpio_val);
  assert_false(ret < 0);

  /* Read Pin Test */

  ret = ioctl(fd_in, GPIOC_READ, &invalue);
  assert_false(ret < 0);

  printf("[Loop test]  outvalue is %d, invalue is %d\n",
         gpio->gpio_val, invalue);
  assert_int_equal(gpio->gpio_val, invalue);

  close(fd_in);
  close(fd_out);
}

/****************************************************************************
 * Name: drivertest_gpio_rw
 ****************************************************************************/

static void drivertest_gpio_rw(FAR void **state)
{
  FAR struct gpio_test_s *gpio = *state;
  char outvalue;
  char invalue;
  int fd;
  int ret;

  fd = open(gpio->gpio_output, O_RDWR);
  assert_false(fd < 0);

  ret = ioctl(fd, GPIOC_SETPINTYPE, GPIO_OUTPUT_PIN);
  assert_false(ret < 0);

  outvalue = gpio_test_randbin() + '0';
  ret = write(fd, &outvalue, sizeof(bool));
  assert_false(ret < 0);

  ret = lseek(fd, 0, SEEK_SET);
  assert_false(ret < 0);

  ret = read(fd, &invalue, sizeof(bool));
  assert_false(ret < 0);

  printf("[input and output test]  outvalue is %d, invalue is %d\n",
          outvalue, invalue);
  assert_int_equal(invalue, outvalue);

  close(fd);
}

/****************************************************************************
 * Name: drivertest_gpio_trigger
 ****************************************************************************/

static FAR void *drivertest_gpio_trigger(FAR void *arg)
{
  FAR struct gpio_test_s *gpio = arg;
  int fd_out;
  int ret;

  fd_out = open(gpio->gpio_output, O_RDWR);
  assert_false(fd_out < 0);

  ret = ioctl(fd_out, GPIOC_SETPINTYPE, (unsigned long) GPIO_OUTPUT_PIN);
  assert_false(ret < 0);

  ret = ioctl(fd_out, GPIOC_WRITE, 0);
  assert_false(ret < 0);
  usleep(300000);
  ret = ioctl(fd_out, GPIOC_WRITE, 1);
  assert_false(ret < 0);
  sleep(2);

  close(fd_out);
  return NULL;
}

/****************************************************************************
 * Name: drivertest_gpio_interrupt
 ****************************************************************************/

static void drivertest_gpio_interrupt(FAR void **state)
{
  FAR struct gpio_test_s *gpio = *state;
  struct pollfd fds;
  pthread_t tid;
  int fd_in;
  int ret;

  ret = pthread_create(&tid, NULL, drivertest_gpio_trigger, gpio);
  assert_false(ret < 0);

  fd_in = open(gpio->gpio_input, O_RDWR | O_NONBLOCK);
  assert_false(fd_in < 0);

  switch (gpio->irq_type)
    {
      case 0:
        ret = ioctl(fd_in, GPIOC_SETPINTYPE, GPIO_INTERRUPT_FALLING_PIN);
        break;
      case 1:
        ret = ioctl(fd_in, GPIOC_SETPINTYPE, GPIO_INTERRUPT_RISING_PIN);
        break;
      case 2:
        ret = ioctl(fd_in, GPIOC_SETPINTYPE, GPIO_INTERRUPT_BOTH_PIN);
        break;
      case 3:
        ret = ioctl(fd_in, GPIOC_SETPINTYPE, GPIO_INTERRUPT_HIGH_PIN);
        break;
      default:
        goto exit_out;
    }

  assert_false(ret < 0);

  fds.events = POLLIN;
  fds.fd = fd_in;
  ret = ioctl(fd_in, GPIOC_REGISTER, NULL);
  assert_false(ret < 0);

  /* timeout set 2000ms */

  ret = poll(&fds, 1, 2000);
  assert_false(ret < 0);

  ret = ioctl(fd_in, GPIOC_UNREGISTER, NULL);
  assert_false(ret < 0);

exit_out:
  pthread_join(tid, NULL);
  close(fd_in);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: drivertest_gpio_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct gpio_test_s gpio =
    {
      .gpio_input    = CONFIG_TESTING_DRIVER_GPIO_INPUT_PIN,
      .gpio_output   = CONFIG_TESTING_DRIVER_GPIO_OUTPUT_PIN,
      .gpio_val      = true,
      .irq_type      = CONFIG_TESTING_DRIVER_GPIO_INTERRUPT_TYPE,
      .loop          = CONFIG_TESTING_DRIVER_GPIO_LOOP,
    };

  parse_commandline(argc, argv, &gpio);

  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test_prestate_setup_teardown(drivertest_gpio_bool,
                                               setup, teardown,
                                               &gpio),
      cmocka_unit_test_prestate_setup_teardown(drivertest_gpio_loop,
                                               setup, teardown,
                                               &gpio),
      cmocka_unit_test_prestate_setup_teardown(drivertest_gpio_rw,
                                                setup, teardown,
                                                &gpio),
      cmocka_unit_test_prestate_setup_teardown(drivertest_gpio_interrupt,
                                               setup, teardown,
                                               &gpio),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
