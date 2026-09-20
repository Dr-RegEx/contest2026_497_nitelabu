/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_cpufreq.c
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include <pthread.h>
#include <nuttx/cpufreq.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TEST_CPUFREQ_FAKE_DRIVER
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))

/****************************************************************************
 * Private Functions Prototypes
 ****************************************************************************/

#ifdef TEST_CPUFREQ_FAKE_DRIVER
static FAR const uint32_t *test_cpufreq_get_table(
                            FAR struct cpufreq_policy *policy);
static int test_cpufreq_target_index(FAR struct cpufreq_policy *policy,
                                     unsigned int index);
static int test_cpufreq_get_frequency(FAR struct cpufreq_policy *policy);
static int test_cpufreq_suspend(FAR struct cpufreq_policy *policy);
static int test_cpufreq_resume(FAR struct cpufreq_policy *policy);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int g_test_cpufreq_current;

static const uint32_t g_test_cpufreq_table[] =
{
  100, 300, 500, 700, 900, CPUFREQ_TABLE_END,
};

static struct cpufreq_driver g_test_cpufreq_driver =
{
    test_cpufreq_get_table,
    test_cpufreq_target_index,
    test_cpufreq_get_frequency,
    test_cpufreq_suspend,
    test_cpufreq_resume,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static FAR const uint32_t *test_cpufreq_get_table(
                            FAR struct cpufreq_policy *policy)
{
  return g_test_cpufreq_table;
}

static int test_cpufreq_target_index(FAR struct cpufreq_policy *policy,
                                     unsigned int index)
{
  DEBUGASSERT(index >= 0 && index < ARRAY_SIZE(g_test_cpufreq_table));
  g_test_cpufreq_current = g_test_cpufreq_table[index];

  return 0;
}

static int test_cpufreq_get_frequency(FAR struct cpufreq_policy *policy)
{
  return g_test_cpufreq_current;
}

static int test_cpufreq_suspend(FAR struct cpufreq_policy *policy)
{
  return 0;
}

static int test_cpufreq_resume(FAR struct cpufreq_policy *policy)
{
  return 0;
}

static int test_cpufreq_fake_driver_init(void)
{
  return cpufreq_init(&g_test_cpufreq_driver);
}

#else /* TEST_CPUFREQ_FAKE_DRIVER */
# define test_cpufreq_fake_driver_init()
#endif /* TEST_CPUFREQ_FAKE_DRIVER */

static int notifier(FAR struct notifier_block *nb,
                    unsigned long action, FAR void *data)
{
  return 0;
}

static FAR void *test_cpufreq_thread(void *arg)
{
  FAR struct cpufreq_policy *policy = cpufreq_policy_get();
  FAR struct qos_request_s *qos;
  struct notifier_block nb;
  int n = 30;

  nb.notifier_call = notifier;
  cpufreq_register_notifier(policy, &nb);

  while (n--)
    {
      int m = 200;

      qos = cpufreq_qos_add_request(policy, 550, 770);
      if (!qos)
        {
          return NULL;
        }

      while (m--)
        {
          int min = random() % 1000;
          int max = random() % 1000 + min;

          cpufreq_qos_update_request(qos, min, max);
        }

      cpufreq_qos_remove_request(qos);
    }

  cpufreq_unregister_notifier(policy, &nb);

  return NULL;
}

static void drivertest_cpufreq(FAR void **state)
{
  FAR struct cpufreq_policy *policy = cpufreq_policy_get();
  pthread_t thread0, thread1, thread2;
  pthread_attr_t attr;

  pthread_attr_init(&attr);

  pthread_create(&thread0, &attr, test_cpufreq_thread, "thread0");
  pthread_create(&thread1, &attr, test_cpufreq_thread, "thread1");
  pthread_create(&thread2, &attr, test_cpufreq_thread, "thread1");

  cpufreq_suspend(policy);
  cpufreq_resume(policy);

  pthread_join(thread0, NULL);
  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);
}

static int setup(FAR void **state)
{
  return test_cpufreq_fake_driver_init();
}

static int teardown(FAR void **state)
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
      cmocka_unit_test_prestate_setup_teardown(drivertest_cpufreq, setup,
                                               teardown, NULL),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
