/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_devfreq.c
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
#include <nuttx/devfreq.h>
#include <sys/param.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct test_devfreq_s
{
  FAR struct devfreq_s *devfreq;
  const char name[NAME_MAX];
  uint32_t cur_freq;
  spinlock_t lock;
};

/****************************************************************************
 * Private Functions Prototypes
 ****************************************************************************/

static FAR const uint32_t *test_devfreq_get_table(
                            FAR struct devfreq_s *devfreq);
static int test_devfreq_target_index(FAR struct devfreq_s *devfreq,
                                     size_t index);
static uint32_t test_devfreq_get_frequency(FAR struct devfreq_s *devfreq);
static int test_devfreq_suspend(FAR struct devfreq_s *devfreq);
static int test_devfreq_resume(FAR struct devfreq_s *devfreq);
static uint32_t test_devfreq_limit(FAR struct devfreq_s *devfreq);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct test_devfreq_s g_test_devfreq =
{
  .devfreq = NULL,
  .name = "test_devfreq",
  .cur_freq = 0,
};

static const uint32_t g_test_devfreq_table[] =
{
  100, 300, 500, 700, 900, DEVFREQ_ENTRY_END
};

static struct devfreq_driver_s g_test_devfreq_driver =
{
  test_devfreq_get_table,
  test_devfreq_target_index,
  test_devfreq_get_frequency,
  test_devfreq_suspend,
  test_devfreq_resume,
};

static struct devfreq_governor_s g_test_devfreq_governor =
{
  .name = "test_devfreq_governor",
  .init = NULL,
  .exit = NULL,
  .start = NULL,
  .stop = NULL,
  .limit = test_devfreq_limit,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static FAR const uint32_t *test_devfreq_get_table(
                            FAR struct devfreq_s *devfreq)
{
  return g_test_devfreq_table;
}

static int test_devfreq_target_index(FAR struct devfreq_s *devfreq,
                                     size_t index)
{
  DEBUGASSERT(index >= 0 && index < nitems(g_test_devfreq_table));
  g_test_devfreq.cur_freq = g_test_devfreq_table[index];

  return 0;
}

static uint32_t test_devfreq_get_frequency(FAR struct devfreq_s *devfreq)
{
  return g_test_devfreq.cur_freq;
}

static int test_devfreq_suspend(FAR struct devfreq_s *devfreq)
{
  return 0;
}

static int test_devfreq_resume(FAR struct devfreq_s *devfreq)
{
  return 0;
}

static uint32_t test_devfreq_limit(FAR struct devfreq_s *devfreq)
{
  return (devfreq->max + devfreq->min) / 2;
}

static int notifier(FAR struct notifier_block *nb,
                    unsigned long action, FAR void *data)
{
  struct devfreq_notifier_s *freqs = data;

  syslog(0, "%s, action %ld, old %"PRIu32", new %"PRIu32", line %d\n",
         __func__, action, freqs->old, freqs->new, __LINE__);

  return 0;
}

static FAR void *test_devfreq_thread(void *arg)
{
  FAR struct devfreq_s *devfreq = arg;
  FAR struct qos_request_s *qos;
  struct notifier_block nb;
  int n = 20;

  nb.notifier_call = notifier;
  devfreq_register_notifier(devfreq, &nb);

  while (n--)
    {
      int m = 50;

      qos = devfreq_qos_add_request(devfreq, 550, 770);
      if (!qos)
        {
          goto out;
        }

      while (m--)
        {
          int min = random() % 1000;
          int max = random() % 1000 + min;

          devfreq_qos_update_request(devfreq, qos, min, max);
          usleep(1000);
        }

      devfreq_qos_remove_request(devfreq, qos);
      sleep(1);
    }

out:
  devfreq_unregister_notifier(devfreq, &nb);
  return NULL;
}

static void drivertest_devfreq(FAR void **state)
{
  FAR struct devfreq_s *devfreq;
  pthread_t thread0, thread1, thread2;
  pthread_attr_t attr;

  devfreq = devfreq_register(g_test_devfreq.name,
                             &g_test_devfreq_governor,
                             &g_test_devfreq_driver,
                             &g_test_devfreq);

  if (!devfreq)
    {
      devfreq = devfreq_find_by_name(g_test_devfreq.name);
      if (!devfreq)
        {
          fail();
        }

      syslog(0, "devfreq %s already exists\n", g_test_devfreq.name);
    }

  pthread_attr_init(&attr);

  pthread_create(&thread0, &attr, test_devfreq_thread, devfreq);
  sleep(1);
  pthread_create(&thread1, &attr, test_devfreq_thread, devfreq);
  sleep(1);
  pthread_create(&thread2, &attr, test_devfreq_thread, devfreq);
  sleep(3);

  devfreq_suspend(devfreq);
  sleep(3);
  devfreq_resume(devfreq);

  pthread_join(thread0, NULL);
  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  devfreq_unregister(devfreq);
}

static int setup(FAR void **state)
{
  return 0;
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
      cmocka_unit_test_prestate_setup_teardown(drivertest_devfreq, setup,
                                               teardown, NULL),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
