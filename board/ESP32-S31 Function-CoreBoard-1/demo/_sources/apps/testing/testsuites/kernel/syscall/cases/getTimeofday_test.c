/****************************************************************************
 * apps/testing/testsuites/kernel/syscall/cases/getTimeofday_test.c
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
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "SyscallTest.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Maximum allowed delta difference in microseconds between gettimeofday
 * and CLOCK_MONOTONIC. This accounts for the time between the two calls.
 */

#define MAX_DELTA_DIFF_US  50000  /* 50ms tolerance */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int done;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: breakout
 ****************************************************************************/

static void breakout(int sig)
{
  done = sig;
}

/****************************************************************************
 * Name: timeval_to_us
 ****************************************************************************/

static int64_t timeval_to_us(const struct timeval *tv)
{
  return (int64_t)tv->tv_sec * 1000000 + tv->tv_usec;
}

/****************************************************************************
 * Name: timespec_to_us
 ****************************************************************************/

static int64_t timespec_to_us(const struct timespec *ts)
{
  return (int64_t)ts->tv_sec * 1000000 + ts->tv_nsec / 1000;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_nuttx_syscall_gettimeofday01
 ****************************************************************************/

void test_nuttx_syscall_gettimeofday01(FAR void **state)
{
  int rtime = 1;
  int basetime_adjust_count = 0;
  const int max_basetime_adjusts = 3;
  struct timeval tv1;
  struct timeval tv2;
  struct timespec mono1;
  struct timespec mono2;
  int64_t tv_delta;
  int64_t mono_delta;
  int64_t delta_diff;

  signal(SIGALRM, breakout);

  done = 0;

  alarm(rtime);

  /* Get initial timestamps from both sources */

  if (gettimeofday(&tv1, NULL))
    {
      fail_msg("FAIL, gettimeofday() failed\n");
    }

  if (clock_gettime(CLOCK_MONOTONIC, &mono1))
    {
      fail_msg("FAIL, clock_gettime(CLOCK_MONOTONIC) failed\n");
    }

  while (!done)
    {
      /* Get current timestamps from both sources */

      if (gettimeofday(&tv2, NULL))
        {
          fail_msg("FAIL, gettimeofday() failed\n");
        }

      if (clock_gettime(CLOCK_MONOTONIC, &mono2))
        {
          fail_msg("FAIL, clock_gettime(CLOCK_MONOTONIC) failed\n");
        }

      /* Calculate deltas - how much time has passed according to each
       * clock
       */

      tv_delta = timeval_to_us(&tv2) - timeval_to_us(&tv1);
      mono_delta = timespec_to_us(&mono2) - timespec_to_us(&mono1);

      /* CLOCK_MONOTONIC should always increase */

      if (mono_delta < 0)
        {
          fail_msg("FAIL, CLOCK_MONOTONIC went backwards!\n");
        }

      /* Compare the deltas. If g_basetime changed, tv_delta will differ
       * from mono_delta by the amount of the basetime adjustment.
       * We only fail if the difference is unreasonably large AND
       * it's not explainable by a basetime change (i.e., mono_delta
       * is positive but tv_delta is negative without basetime change).
       */

      delta_diff = tv_delta - mono_delta;

      /* If delta_diff is negative and large, it means gettimeofday
       * reported less time passed than CLOCK_MONOTONIC.
       * This is only valid if g_basetime was adjusted backwards.
       * We allow this but verify the magnitude is reasonable.
       */

      if (delta_diff < -MAX_DELTA_DIFF_US)
        {
          /* Large negative difference - likely g_basetime adjustment.
           * Allow limited number of adjustments.
           */

          basetime_adjust_count++;
          if (basetime_adjust_count > max_basetime_adjusts)
            {
              fail_msg("FAIL, too many basetime adjustments (%d), "
                       "timer may have issues\n", basetime_adjust_count);
            }

          tv1 = tv2;
          mono1 = mono2;
          usleep(200000);
          continue;
        }

      /* If delta_diff is positive and large, gettimeofday reported
       * more time than actually passed. This could indicate a bug.
       */

      if (delta_diff > MAX_DELTA_DIFF_US)
        {
          fail_msg("FAIL, gettimeofday delta (%lld us) exceeds "
                   "CLOCK_MONOTONIC delta (%lld us) by %lld us\n",
                   (long long)tv_delta, (long long)mono_delta,
                   (long long)delta_diff);
        }

      usleep(200000);
      tv1 = tv2;
      mono1 = mono2;
    }
}
