/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_pm_callback.c
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
#include <nuttx/nuttx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include <nuttx/power/pm.h>
#include <sys/param.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PM_CALLBACK_SET_PRIO(cb, prio) \
  do \
    { \
      (cb)->prio = (prio); \
    } \
  while (0)

#define TEST_PM_LOOP_COUNT  (1)
#define TEST_DOMAIN         (0)
#define TEST_WAITTIME       (1000)  /* in us */
#define TEST_PRIO           (1)

/****************************************************************************
 * Private Functions Prototypes
 ****************************************************************************/

static void notify_child(FAR struct pm_callback_s *cb,
                         int domain,
                         enum pm_state_e pmstate);
static void notify_parent(FAR struct pm_callback_s *cb,
                          int domain,
                          enum pm_state_e pmstate);

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct test_pm_reg_prio_s
{
  enum pm_state_e state;
  int test_result;
  bool notify_parent_once;
  bool notify_child_once;
  struct pm_callback_s callback_child;
  struct pm_callback_s callback_parent;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int test_pm_reset(struct test_pm_reg_prio_s *dev, int state)
{
  dev->test_result         = 0;
  dev->notify_child_once   = false;
  dev->notify_parent_once  = false;
  dev->state               = state;
  return 0;
}

static void notify_child(FAR struct pm_callback_s *cb,
                         int domain,
                         enum pm_state_e pmstate)
{
  struct test_pm_reg_prio_s *dev;
  dev = container_of(cb, struct test_pm_reg_prio_s, callback_child);

  if (pmstate == PM_RESTORE)
    {
      return;
    }

  if (!dev->notify_child_once && pmstate != dev->state)
    {
      dev->test_result += 1;
      dev->notify_child_once = true;
    }
}

static void notify_parent(FAR struct pm_callback_s *cb,
                          int domain,
                          enum pm_state_e pmstate)
{
  struct test_pm_reg_prio_s *dev;
  dev = container_of(cb, struct test_pm_reg_prio_s, callback_parent);

  if (pmstate == PM_RESTORE)
    {
      return;
    }

  if (!dev->notify_parent_once && pmstate != dev->state)
    {
      dev->test_result *= 10;
      dev->notify_parent_once = true;
    }
}

static void drivertest_pm_callback(FAR void **argv)
{
  int ret                         = 0;
  int cnt                         = TEST_PM_LOOP_COUNT;
  int prio                        = TEST_PRIO;
  struct test_pm_reg_prio_s *dev  = *argv;
  int init_delay;

  if (CONFIG_PM_GOVERNOR_EXPLICIT_RELAX < 0)
    {
      init_delay = 0;
    }
  else
    {
      init_delay = MAX(CONFIG_PM_GOVERNOR_EXPLICIT_RELAX,
                       CONFIG_SERIAL_PM_ACTIVITY_PRIORITY);
    }

  usleep(init_delay * 1000000);

  while (cnt--)
    {
      /* suppose register parent's callback first */

      ret = pm_domain_register(TEST_DOMAIN, &dev->callback_parent);
      assert_int_equal(ret, 0);
      ret = pm_domain_register(TEST_DOMAIN, &dev->callback_child);
      assert_int_equal(ret, 0);

      pm_stay(TEST_DOMAIN, PM_NORMAL);
      usleep(TEST_WAITTIME);
      test_pm_reset(dev, PM_NORMAL);

      /* when suspend， call child's callback first */

      pm_relax(TEST_DOMAIN, PM_NORMAL);
      usleep(TEST_WAITTIME);

      assert_int_equal(dev->test_result, 10);

      /* when resume, call parent's callback first */

      test_pm_reset(dev, PM_SLEEP);
      pm_stay(TEST_DOMAIN, PM_NORMAL);
      usleep(TEST_WAITTIME);

      assert_int_equal(dev->test_result, 1);

      pm_relax(TEST_DOMAIN, PM_NORMAL);

      ret = pm_domain_unregister(TEST_DOMAIN, &dev->callback_child);
      assert_int_equal(ret, 0);
      ret = pm_domain_unregister(TEST_DOMAIN, &dev->callback_parent);
      assert_int_equal(ret, 0);

      /* set child prio 1, and parent is default 0 */

      PM_CALLBACK_SET_PRIO(&dev->callback_child, prio);

      /* if set priority, when resume, call higher priority callback first */

      ret = pm_domain_register(TEST_DOMAIN, &dev->callback_parent);
      assert_int_equal(ret, 0);
      ret = pm_domain_register(TEST_DOMAIN, &dev->callback_child);
      assert_int_equal(ret, 0);

      pm_stay(TEST_DOMAIN, PM_NORMAL);
      usleep(TEST_WAITTIME);
      test_pm_reset(dev, PM_NORMAL);

      /* when suspend, call parent's callback first */

      pm_relax(TEST_DOMAIN, PM_NORMAL);
      usleep(TEST_WAITTIME);

      assert_int_equal(dev->test_result, 1);

      /* when resume，still call child's callback first */

      test_pm_reset(dev, PM_SLEEP);
      pm_stay(TEST_DOMAIN, PM_NORMAL);
      usleep(TEST_WAITTIME);

      assert_int_equal(dev->test_result, 10);

      pm_relax(TEST_DOMAIN, PM_NORMAL);

      ret = pm_domain_unregister(TEST_DOMAIN, &dev->callback_child);
      assert_int_equal(ret, 0);
      ret = pm_domain_unregister(TEST_DOMAIN, &dev->callback_parent);
      assert_int_equal(ret, 0);
    }
}

static int setup(FAR void **argv)
{
  struct test_pm_reg_prio_s *dev = *argv;
  dev->callback_child.notify = notify_child;
  dev->callback_parent.notify = notify_parent;

  return 0;
}

static int teardown(FAR void **argv)
{
  struct test_pm_reg_prio_s *dev = *argv;

  /* if case assert, need to unregister callback.
   * if already unregister, unregister again may cause segmentation fault.
   * But register twice is not a problem, just return -EExist.
   */

  pm_domain_register(TEST_DOMAIN, &dev->callback_child);
  pm_domain_unregister(TEST_DOMAIN, &dev->callback_child);
  pm_domain_register(TEST_DOMAIN, &dev->callback_parent);
  pm_domain_unregister(TEST_DOMAIN, &dev->callback_parent);
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct test_pm_reg_prio_s dev;
  memset(&dev, 0, sizeof(dev));

  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test_prestate_setup_teardown(drivertest_pm_callback, setup,
                                               teardown, &dev),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
