/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_pm.c
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

#define ASSERT_LESS_EQUAL_IF_CHECK(actual, expected, check) \
  { \
    if (check) \
      { \
        assert_true(actual <= expected); \
      } \
  }

#define ASSERT_LESS_EQUAL(actual, expected) \
  { \
    assert_true(actual <= expected); \
  }

#define TEST_PM_LOOP_COUNT 1

/* please set num of domain by menuconfig, ensure TEST_DOMAIN is legal
 * (TEST_DOMAIN < CONFIG_PM_NDOMAINS) and no driver or module use it
 */

#define TEST_DOMAIN 0
#define TEST_STAYTIMEOUT 10 /* in ms */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct pm_wakelock_s g_test_wakelock[PM_COUNT];
static char test_wakelock_name[PM_COUNT][32] =
{
  "test_pm_wakelock_normal",
  "test_pm_wakelock_idle",
  "test_pm_wakelock_standby",
  "test_pm_wakelock_sleep"
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void test_pm_stay(int domain, int state, bool check)
{
  int staycount;
  int newstate;

  staycount = pm_staycount(domain, state);

  pm_stay(domain, state);

  newstate = pm_checkstate(domain);
  ASSERT_LESS_EQUAL_IF_CHECK(newstate, state, check);
  ASSERT_LESS_EQUAL_IF_CHECK(pm_staycount(domain, state),
                             staycount + 1, check);
}

static void test_pm_relax(int domain, int state, bool check)
{
  int staycount = pm_staycount(domain, state);

  pm_relax(domain, state);
  ASSERT_LESS_EQUAL_IF_CHECK(pm_staycount(domain, state),
                             staycount - 1, check);
}

static void test_pm_staytimeout(int domain, int state, bool check)
{
  int staycount;
  int newstate;
  irqstate_t flag;

  flag = up_irq_save();

  staycount = pm_staycount(domain, state);

  pm_staytimeout(domain, state, TEST_STAYTIMEOUT);

  newstate = pm_checkstate(domain);
  ASSERT_LESS_EQUAL_IF_CHECK(newstate, state, check);
  ASSERT_LESS_EQUAL_IF_CHECK(pm_staycount(domain, state),
                             staycount + 1, check);

  up_irq_restore(flag);

  usleep(TEST_STAYTIMEOUT * 1000);
  ASSERT_LESS_EQUAL_IF_CHECK(pm_staycount(domain, state), staycount, check);
}

static void test_pm_wakelock_stay(int domain, int state, bool check)
{
  int staycount;
  int newstate;

  staycount = pm_wakelock_staycount(&g_test_wakelock[state]);

  pm_wakelock_stay(&g_test_wakelock[state]);

  newstate = pm_checkstate(domain);
  ASSERT_LESS_EQUAL_IF_CHECK(newstate, state, check);
  ASSERT_LESS_EQUAL_IF_CHECK(pm_wakelock_staycount(&g_test_wakelock[state]),
                             staycount + 1, check);
}

static void test_pm_wakelock_relax(int domain, int state, bool check)
{
  int staycount = pm_wakelock_staycount(&g_test_wakelock[state]);

  pm_wakelock_relax(&g_test_wakelock[state]);
  ASSERT_LESS_EQUAL_IF_CHECK(pm_wakelock_staycount(&g_test_wakelock[state]),
                             staycount - 1, check);
}

static void test_pm_wakelock_staytimeout(int domain, int state, bool check)
{
  int staycount;
  int newstate;
  irqstate_t flag;

  flag = up_irq_save();

  staycount = pm_wakelock_staycount(&g_test_wakelock[state]);

  pm_wakelock_staytimeout(&g_test_wakelock[state], TEST_STAYTIMEOUT);

  newstate = pm_checkstate(domain);
  ASSERT_LESS_EQUAL_IF_CHECK(newstate, state, check);
  ASSERT_LESS_EQUAL_IF_CHECK(pm_wakelock_staycount(&g_test_wakelock[state]),
                             staycount + 1, check);

  up_irq_restore(flag);

  usleep(TEST_STAYTIMEOUT * 1000);
  ASSERT_LESS_EQUAL_IF_CHECK(pm_wakelock_staycount(&g_test_wakelock[state]),
                             staycount, check);
}

static void drivertest_pm(FAR void **argv)
{
  int init_delay;
  bool check;
  int domain = TEST_DOMAIN;
  int ret    = 0;
  int cnt    = TEST_PM_LOOP_COUNT;

  if (CONFIG_PM_GOVERNOR_EXPLICIT_RELAX < 0)
    {
      check = false;
      init_delay = 0;
    }
  else
    {
      check = true;
      init_delay = MAX(CONFIG_PM_GOVERNOR_EXPLICIT_RELAX,
                       CONFIG_SERIAL_PM_ACTIVITY_PRIORITY);
    }

  usleep(init_delay * 1000000);
  pm_auto_update(domain, true);

  while (cnt--)
    {
      assert_int_equal(ret, 0);

      for (int state = 0; state < PM_COUNT; state++)
        {
          test_pm_stay(domain, state, check);
          test_pm_relax(domain, state, check);
#ifndef CONFIG_PM_GOVERNOR_STABILITY
          test_pm_staytimeout(domain, state, check);
#endif
        }

      for (int state = 0; state < PM_COUNT; state++)
        {
          test_pm_wakelock_stay(domain, state, check);
          test_pm_wakelock_relax(domain, state, check);
#ifndef CONFIG_PM_GOVERNOR_STABILITY
          test_pm_wakelock_staytimeout(domain, state, check);
#endif
        }

      assert_int_equal(ret, 0);
    }
}

static int setup(FAR void **argv)
{
  for (int state = 0; state < PM_COUNT; state++)
    {
      pm_wakelock_init(&g_test_wakelock[state],
                       test_wakelock_name[state],
                       TEST_DOMAIN,
                       state);
    }

  return 0;
}

static int teardown(FAR void **argv)
{
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test_prestate_setup_teardown(drivertest_pm, setup,
                                               teardown, NULL),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
