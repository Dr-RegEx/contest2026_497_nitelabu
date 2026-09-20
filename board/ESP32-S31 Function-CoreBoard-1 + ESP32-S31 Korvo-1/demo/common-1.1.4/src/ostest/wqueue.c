/****************************************************************************
 * apps/testing/ostest/wqueue.c
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
#include <nuttx/wqueue.h>
#include <nuttx/atomic.h>

#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

#include "ostest.h"

#ifdef CONFIG_SCHED_WORKQUEUE

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define wqtest_assert(f, msg) __ASSERT__(f, __FILE__, __LINE__, msg)

#define SLEEP_TIME   (10 * 1000)
#define TEST_COUNT   (20)
#define VERIFY_COUNT (20)

#define WQUEUE_DEFAULT_STACK_SIZE (CONFIG_DEFAULT_TASK_STACKSIZE)
#define WQUEUE_DEFAULT_PRIORITY    PRIORITY
#define WQUEUE_DEFAULT_THREAD_NUM (2)

#define WQUEUE_MAX_DELAY   WDOG_MAX_DELAY

#define PERIODIC_TEST_WORK_NUM    (5)
#define PERIODIC_TEST_PERIOD_BASE MSEC2TICK(20)
#define PERIODIC_TEST_PERIOD_STEP MSEC2TICK(2)
#define PERIODIC_TEST_MIN_LOOP    (5)

#define PERIODIC_TEST_PERIOD_MAX                                             \
  (PERIODIC_TEST_PERIOD_BASE                                                 \
   + PERIODIC_TEST_PERIOD_STEP * PERIODIC_TEST_WORK_NUM)

#define RECURSIVE_TEST_MAXDEPTH          (3)
#define RECURSIVE_TEST_SUBNODE_NUM       (3)
#define RECURSIVE_TEST_PRINT_PATH_ENABLE (0)

#define DELAY_TEST_NUM               (20)
#define DELAY_TEST_DEFAULT_DELAY     ((clock_t)400)
#define DELAY_TEST_THREAD_NUM        (5)
#define DELAY_TEST_WQUEUE_NUM        (3)
#define DELAY_TEST_WQUEUE_THREAD_NUM (3)
#define DELAY_TEST_TIMEOUT_MS        (2 * 1000)
#define DELAY_TEST_DELAY_MAX         (500)
#define DELAY_TEST_DELAY_MIN         (50)

#define WORK_THREAD_TEST_THREAD_NUM           (4)
#define WORK_THREAD_TEST_MAX_LOOP             (20)
#define WORK_THREAD_TEST_WORK_SLEEP_TIME_MS   (50)
#define WORK_THREAD_TEST_EFFICIENCY_THRESHOLD (80)

#define MAX_CLOCK_ERROR ((clock_t)3000)

#ifdef CONFIG_SCHED_LPWORK
#  define TEST_QUEUE           LPWORK
#  define TEST_QUEUE_PRIORITY  CONFIG_SCHED_LPWORKPRIORITY
#else
#  define TEST_QUEUE           HPWORK
#  define TEST_QUEUE_PRIORITY  CONFIG_SCHED_HPWORKPRIORITY
#endif

#define WORK_NOTIFIER_TEST_TIMEOUT_US 100000
#define FD1                           10
#define FD2                           11

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct delays_s
{
  size_t num;
  FAR const clock_t *data;
} delays_t;

typedef struct special_delay_s
{
  size_t num;
  FAR const delays_t *delays;
} special_delay_t;

typedef struct recursive_work_s
{
  FAR struct kwork_wqueue_s *wq;
  struct work_s work;

  int depth;
#if RECURSIVE_TEST_PRINT_PATH_ENABLE
  int path[RECURSIVE_TEST_MAXDEPTH]; /* recursive path */
#endif
  FAR sem_t *cnt_sem;
} recursive_work_t;

typedef struct period_work_s
{
  struct work_s work;
  FAR struct kwork_wqueue_s *wq;
  FAR atomic_t *runing_cnt;
  FAR atomic_t *enough_cnt;
  clock_t period;
  clock_t prev_tick;
  clock_t total_tick;
  int cnt;
  bool is_first;
} period_work_t;

typedef struct delay_work_s
{
  FAR struct kwork_wqueue_s *wq;
  struct work_s work;
  bool is_finish;
  bool is_valid;
  int wqid;
  int id;
  clock_t delay;
  clock_t added_tick;
  clock_t start_tick;
  FAR sem_t *finish_sem;
} delay_work_t;

typedef struct workthread_work_s
{
  sem_t finish_sem;
  atomic_t total_ticks;
  FAR struct kwork_wqueue_s *wq;
  FAR struct work_s *works;
  atomic_t pending_cnt;
  atomic_t total_cnt;
  atomic_t finish_cnt;
  int nthread;
  int nworks;
  bool is_first;
} workthread_work_t;

typedef struct
{
  int id;
  int nwq;
  int nthreads;
  int nworks;
  FAR struct kwork_wqueue_s **wq; /* struct kwork_wqueue_s *wq[nwq] */
  FAR delay_work_t *works;        /* delay_work_t works[nworks] */
} delay_work_adder_thread_arg_t;

struct test_work1_s
{
  bool completed;
};

struct test_work2_s
{
  bool completed;
  bool first_notify;
};

struct test_state_s
{
  struct test_work1_s test_work1;
  struct test_work2_s test_work2;
};

/****************************************************************************
 * Private Functions Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const clock_t delay_caes_0[2] =
{
  CLOCK_MAX, 1
};

static const clock_t delay_case_1[2] =
{
  CLOCK_MAX, (clock_t)WQUEUE_MAX_DELAY
};

static const delays_t delay_cases[2] =
{
  { 2, delay_caes_0 },
  { 2, delay_case_1 },
};

static const special_delay_t special_delays_test_arg =
{
  2, delay_cases
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline_function clock_t clock_diff(clock_t a, clock_t b)
{
  return a > b ? a - b : b - a;
}

static inline_function int get_priority_self(void)
{
  int priority = 0;
  struct sched_param param;
  int policy;
  if (pthread_getschedparam(pthread_self(), &policy, &param) == 0)
    {
      priority = param.sched_priority;
    }

  return priority;
}

static void empty_worker(FAR void *arg)
{
}

static void sleep_worker(FAR void *arg)
{
  FAR sem_t *sem = arg;
  usleep(SLEEP_TIME);
  sem_post(sem);
}

static void count_worker(FAR void *arg)
{
  FAR sem_t *sem = arg;
  sem_post(sem);
}

static void period_worker(FAR void *arg)
{
  FAR period_work_t *p = arg;
  clock_t tick_now     = clock_systime_ticks();
  if (p->is_first)
    {
      p->prev_tick  = tick_now;
      p->is_first   = false;
    }
  else
    {
      p->total_tick += tick_now - p->prev_tick;
      p->prev_tick = tick_now;
      p->cnt++;
    }

  atomic_add(p->runing_cnt, 1);

  if (p->cnt == PERIODIC_TEST_WORK_NUM)
    {
      atomic_add(p->enough_cnt, 1);
    }

  work_queue_next_wq(p->wq, &p->work, period_worker, arg, p->period);
}

static void recursive_worker(FAR void *arg)
{
  FAR recursive_work_t *work = arg;
  int i;

#if RECURSIVE_TEST_PRINT_PATH_ENABLE
  printf("Recursive worker, path: X");
  for (i = 0; i < p->depth; i++)
    {
      printf("->%d", p->path[i]);
    }

  printf("\n");
#endif

  sem_post(work->cnt_sem);

  if (work->depth < RECURSIVE_TEST_MAXDEPTH)
    {
      for (i = 0; i < RECURSIVE_TEST_SUBNODE_NUM; i++)
        {
          FAR recursive_work_t *child_work
              = malloc(sizeof(recursive_work_t));
          if (child_work == NULL)
            {
              printf("wqueue_test: malloc failed\n");
              continue;
            }

          memcpy(child_work, work, sizeof(recursive_work_t));
          memset(&child_work->work, 0, sizeof(struct work_s));
          child_work->depth             = work->depth + 1;
          child_work->wq                = work->wq;
#if RECURSIVE_TEST_PRINT_PATH_ENABLE
          child_work->path[work->depth] = i;
#endif
          work_queue_wq(child_work->wq, &child_work->work, recursive_worker,
                        child_work, 0);
        }
    }

  free(arg);
}

static void delay_worker(FAR void *arg)
{
  FAR delay_work_t *p = arg;
  p->start_tick       = clock_systime_ticks();
  p->is_finish        = true;
  sem_post(p->finish_sem);
}

static void workthread_worker(FAR void *arg)
{
  FAR workthread_work_t *work = (workthread_work_t *)arg;
  clock_t start_tick;

  COMPILE_TIME_ASSERT(WORK_THREAD_TEST_WORK_SLEEP_TIME_MS < 1000);
  start_tick = clock_systime_ticks();

  if (!work->is_first)
    {
      usleep(WORK_THREAD_TEST_WORK_SLEEP_TIME_MS * 1000);
      atomic_sub(&work->pending_cnt, 1);
    }
  else
    {
      work->is_first = false;
    }

  while (atomic_read(&work->pending_cnt) <= work->nthread)
    {
      int curr_total;
      int next_total;

      curr_total = atomic_read(&work->total_cnt);
      if (curr_total >= work->nworks)
        break;

      next_total = curr_total + 1;
      if (atomic_cmpxchg(&work->total_cnt, &curr_total, next_total))
        {
          work_queue_wq(work->wq, &work->works[next_total],
                        workthread_worker, work, 0);
          atomic_add(&work->pending_cnt, 1);
        }
    }

  if (!work->is_first)
    {
      clock_t exec_tick;
      int finish_cnt;

      exec_tick  = clock_systime_ticks() - start_tick;
      finish_cnt = atomic_add(&work->finish_cnt, 1);
      atomic_add(&work->total_ticks, exec_tick);
      if (finish_cnt + 1 == work->nworks)
        {
          sem_post(&work->finish_sem);
        }
    }
}

static void *delay_work_adder_thread(FAR void *arg)
{
  FAR delay_work_adder_thread_arg_t *p
      = (delay_work_adder_thread_arg_t *)arg;

  int wq_idx;
  int i;
  int status;

  for (i = p->id; i < p->nworks; i += p->nthreads)
    {
      FAR delay_work_t *work;

      wq_idx           = (i / p->nthreads) % p->nwq;
      work             = &p->works[i];
      work->wq         = p->wq[wq_idx];
      work->wqid       = wq_idx;
      work->id         = i;
      work->is_finish  = false;
      work->added_tick = clock_systime_ticks();
      status = work_queue_wq(p->wq[wq_idx], &work->work, delay_worker, work,
                             work->delay);
      if (status)
        {
          work->is_valid = false;
        }
      else
        {
          work->is_valid = true;
        }

      usleep(USEC_PER_TICK * 100);
    }

  return NULL;
}

static FAR void *tester(FAR void *arg)
{
  FAR void **val = arg;
  struct work_s work;
  int i;

  memset(&work, 0, sizeof(work));
  for (i = 0; i < TEST_COUNT; i++)
    {
      if (val[1] != NULL)
        {
          work_queue_wq(val[1], &work, empty_worker, NULL, 0);
          work_cancel_wq(val[1], &work);
        }
      else
        {
          work_queue((int)(uintptr_t)val[0], &work, empty_worker, NULL, 0);
          work_cancel((int)(uintptr_t)val[0], &work);
        }

      usleep((int)(uintptr_t)val[2]);
    }

  usleep(SLEEP_TIME); /* Wait for workers to run. */
  return NULL;
}

static FAR void *verifier(FAR void *arg)
{
  FAR void **val = arg;
  sem_t sem;
  sem_t call_sem;
  int call_count;
  int i;
  struct work_s work[VERIFY_COUNT + 1];

  sem_init(&sem, 0, 0);
  sem_init(&call_sem, 0, 0);
  memset(&work, 0, sizeof(work));

  /* Queue sleep worker. */

  if (val[1] != NULL)
    {
      work_queue_wq(val[1], &work[0], sleep_worker, &sem, 0);
    }
  else
    {
      work_queue((int)(uintptr_t)val[0], &work[0], sleep_worker, &sem, 0);
    }

  /* Queue count workers when qid is busy. */

  for (i = 1; i <= VERIFY_COUNT; i++)
    {
      if (val[1] != NULL)
        {
          work_queue_wq(val[1], &work[i], count_worker, &call_sem, 0);
        }
      else
        {
          work_queue((int)(uintptr_t)val[0], &work[i],
                     count_worker, &call_sem, 0);
        }
    }

  /* Wait for sleep worker to run. */

  sem_wait(&sem);

  /* Wait for count workers to run. */

  do
    {
      usleep(SLEEP_TIME);
      sem_getvalue(&call_sem, &call_count);
    }
  while (call_count != VERIFY_COUNT);

  sem_getvalue(&call_sem, &call_count);
  printf("wqueue_test: call = %d, expect = %d\n", call_count, VERIFY_COUNT);

  for (i = 0; i < VERIFY_COUNT; i++)
    {
      ASSERT(work[i].worker == NULL);
    }

  ASSERT(call_count == VERIFY_COUNT);
  return NULL;
}

static void run_once(int qid, FAR void *wq, int interval,
                     int priority_test, int priority_verify)
{
  pthread_t thread;
  pthread_attr_t attr;
  struct sched_param sparam;
  int status;
  FAR void *val[3];

  status = pthread_attr_init(&attr);
  if (status != 0)
    {
      printf("wqueue_test: pthread_attr_init failed, status=%d\n", status);
    }

  memset(&sparam, 0, sizeof(sparam));

  /* Tester: try race conditions. */

  sparam.sched_priority = priority_test;
  status = pthread_attr_setschedparam(&attr, &sparam);
  if (status != 0)
    {
      printf("wqueue_test: pthread_attr_setschedparam failed for tester, "
             "status=%d\n", status);
    }

  val[0] = (FAR void *)(uintptr_t)qid;
  val[1] = wq;
  val[2] = (FAR void *)(uintptr_t)interval;
  status = pthread_create(&thread, &attr, tester, val);
  if (status != 0)
    {
      printf("wqueue_test: pthread_create failed for tester, "
             "status=%d\n", status);
    }

  status = pthread_join(thread, NULL);
  if (status != 0)
    {
      printf("wqueue_test: pthread_join failed for tester, "
             "status=%d\n", status);
    }

  /* Verifier: make sure queue is still working properly. */

  sparam.sched_priority = priority_verify;
  status = pthread_attr_setschedparam(&attr, &sparam);
  if (status != 0)
    {
      printf("wqueue_test: pthread_attr_setschedparam failed for verifier, "
             "status=%d\n", status);
    }

  status = pthread_attr_setstacksize(&attr,
        VERIFY_COUNT * sizeof(struct work_s) + CONFIG_PTHREAD_STACK_DEFAULT);
  if (status != 0)
    {
      printf("wqueue_test: pthread_attr_setstacksize failed for verifier, "
             "status=%d\n", status);
    }

  val[0] = (FAR void *)(uintptr_t)qid;
  val[1] = wq;
  status = pthread_create(&thread, &attr, verifier, val);
  if (status != 0)
    {
      printf("wqueue_test: pthread_create failed for verifier, "
             "status=%d\n", status);
    }

  status = pthread_join(thread, NULL);
  if (status != 0)
    {
      printf("wqueue_test: pthread_join failed for verifier, "
             "status=%d\n", status);
    }
}

static void wqueue_priority_test(int qid, FAR void *wq, int prio)
{
  int interval;
  int priority_test;
  int priority_verify;

  for (interval = 0; interval <= 1; interval++)
    {
      for (priority_test  = prio - 1;
           priority_test <= prio + 1;
           priority_test++)
        {
          for (priority_verify  = prio - 1;
               priority_verify <= prio + 1;
               priority_verify++)
            {
              run_once(qid, wq, interval, priority_test, priority_verify);
            }
        }
    }
}

static void wqueue_recursive_test(void)
{
  FAR recursive_work_t *work;
  FAR struct kwork_wqueue_s *wq;
  sem_t cnt_sem;
  int cnt;
  int expect_cnt;
  int i;

  printf("\nwqueue_test: recursive work test "
         "start...\n");

  expect_cnt = 1;
  for (i = 0; i < RECURSIVE_TEST_MAXDEPTH + 1; i++)
    {
      expect_cnt *= RECURSIVE_TEST_SUBNODE_NUM;
    }

  expect_cnt = (expect_cnt - 1) / (RECURSIVE_TEST_SUBNODE_NUM - 1);

  sem_init(&cnt_sem, 0, 0);

  wq = work_queue_create("recursive test", WQUEUE_DEFAULT_PRIORITY, NULL,
                         WQUEUE_DEFAULT_STACK_SIZE,
                         WQUEUE_DEFAULT_THREAD_NUM);
  if (wq == NULL)
    {
      printf("wqueue_test: work_queue_create failed\n");
      return;
    }

  work = zalloc(sizeof(recursive_work_t));
  if (work == NULL)
    {
      printf("wqueue_test: malloc failed\n");
      goto errout;
    }

  work->depth   = 0;
  work->cnt_sem = &cnt_sem;
  work->wq      = wq;

  recursive_worker(work);

  do
    {
      usleep(SLEEP_TIME);
      sem_getvalue(&cnt_sem, &cnt);
    }
  while (cnt != expect_cnt);

  printf("Expect works: %d, actual works: %d\n", expect_cnt, cnt);

  if (cnt != expect_cnt)
    {
      wqtest_assert(false, "wqueue_test: Recursive work test "
                           "failed...\n");
    }
  else
    {
      printf("wqueue_test: recursive work test done.\n");
    }

errout:
  work_queue_free(wq);
}

static void wqueue_period_and_cancel_test(void)
{
  FAR struct kwork_wqueue_s *wq;
  FAR period_work_t *works;
  FAR period_work_t *work;
  atomic_t running_cnt;
  atomic_t enough_cnt;
  int total_cnt        = PERIODIC_TEST_WORK_NUM;
  int success_cnt;
  int prev_cnt;
  int curr_cnt;
  int i;

  atomic_set(&running_cnt, 0);
  atomic_set(&enough_cnt, 0);

  printf("\nwqueue_test: periodic work and cancel test start...\n");
  printf("Testing %d periodic work\n", total_cnt);

  wqtest_assert(PERIODIC_TEST_PERIOD_MAX <= WQUEUE_MAX_DELAY,
                "wqueue_test period exceeds WDOG_MAX_DELAY\n");

  wq = work_queue_create("test", WQUEUE_DEFAULT_PRIORITY - 1, NULL,
                         WQUEUE_DEFAULT_STACK_SIZE,
                         WQUEUE_DEFAULT_THREAD_NUM);
  if (wq == NULL)
    {
      printf("wqueue_test: work_queue_create failed\n");
      return;
    }

  works = (period_work_t *)zalloc((total_cnt) * sizeof(period_work_t));
  if (works == NULL)
    {
      printf("wqueue_test: malloc failed\n");
      goto errout;
    }

  for (i = 0; i < total_cnt; i++)
    {
      work              = &works[i];
      work->wq          = wq;
      work->is_first    = true;
      work->period
          = PERIODIC_TEST_PERIOD_STEP * i + PERIODIC_TEST_PERIOD_BASE;
      work->runing_cnt = &running_cnt;
      work->enough_cnt = &enough_cnt;
      work_queue_wq(wq, &work->work, period_worker, work, 0);
    }

  while (atomic_read(&enough_cnt) != total_cnt)
    {
      usleep(SLEEP_TIME);
    }

  success_cnt = 0;
  for (i = 0; i < total_cnt; i++)
    {
      clock_t period;

      work = &works[i];

      work_cancel_sync_wq(wq, &work->work);

      wqtest_assert(work_available(&work->work) == true,
                    "wqueue_test: a cancelled work's worker is not NULL\n");

      period = work->total_tick / work->cnt;
      if (clock_diff(period, work->period) <= MAX_CLOCK_ERROR)
        {
          success_cnt++;
          printf("[Pass] ");
        }
      else
        {
          printf("[Fail] ");
        }

      printf("Periodic work %d: expected "
             "period: %" PRIuTM ",\t actual period: %" PRIuTM "\n",
             i, work->period, period);
    }

  prev_cnt = atomic_read(&running_cnt);
  usleep(2 * USEC_PER_TICK * PERIODIC_TEST_PERIOD_MAX);
  curr_cnt = atomic_read(&running_cnt);

  printf("cnt_prev: %d, cnt_curr: %d\n", prev_cnt, curr_cnt);
  wqtest_assert(prev_cnt == curr_cnt,
                "wqueue_test: cancel test failed! Some periodic works "
                "are still running\n");

  printf("Passed %d/%d\n", success_cnt, total_cnt);

  wqtest_assert(success_cnt == total_cnt,
                "wqueue_test: period and delay test: some delay or period "
                "is not correct\n");

  printf("wqueue_test: periodic work and cancel test done\n");

  free(works);
errout:
  work_queue_free(wq);
}

static int wqueue_delay_test_base(FAR const clock_t *special_delay,
                                  int n,
                                  int n_special,
                                  clock_t delay_min,
                                  clock_t delay_max,
                                  int nwq,
                                  int nthreads,
                                  int *thread_priorities,
                                  int *wq_priorities,
                                  int timeout_ms)
{
  FAR pthread_t *threads;
  FAR struct kwork_wqueue_s **wqueues;
  FAR delay_work_adder_thread_arg_t *thread_work_adder_args;
  FAR delay_work_t *works;
  pthread_attr_t attr;
  struct sched_param sparam;
  sem_t finish_sem;
  int fail_count       = 0;
  int success_count    = 0;
  int warning_count    = 0;
  int timeout_count    = 0;
  int unexpected_count = 0;
  int invalid_count    = 0;
  int invalid_num;
  int timeout_cnt;
  int i;
  int status;
  int sval;
  int ret = 0;

  DEBUGASSERT(n >= n_special);
  DEBUGASSERT(delay_min < delay_max);

  srand(clock_systime_ticks());
  works   = zalloc(n * sizeof(delay_work_t));
  threads = malloc(nthreads * sizeof(pthread_t));
  wqueues = zalloc(nwq * sizeof(struct kwork_wqueue_s *));
  thread_work_adder_args
      = malloc(nthreads * sizeof(delay_work_adder_thread_arg_t));

  if (works == NULL || threads == NULL || wqueues == NULL
      || thread_work_adder_args == NULL)
    {
      printf("wqueue_test: malloc failed\n");
      goto errout;
    }

  invalid_num = 0;
  for (i = 0; i < n; i++)
    {
      delay_work_t *work = &works[i];
      work->finish_sem   = &finish_sem;
      if (i < n_special)
        {
          work->delay = special_delay[i];
        }
      else
        {
          work->delay = rand() % (delay_max - delay_min) + delay_min;
        }

      if (work->delay > WQUEUE_MAX_DELAY)
        {
          work->is_valid = false;
          invalid_num++;
        }
    }

  sem_init(&finish_sem, 0, invalid_num);

  printf("wqueue_test: delay test\nThreads' Priorities:{%d} [ ", nthreads);
  for (i = 0; i < nthreads; i++)
    {
      printf("%d ", thread_priorities[i]);
    }

  printf("]\nWork Queues' Priorities:{%d} [ ", nwq);
  for (i = 0; i < nwq; i++)
    {
      printf("%d ", wq_priorities[i]);
    }

  printf("]\nDelays:{%d} [ ", n);
  for (i = 0; i < n; i++)
    {
      printf("%" PRIuTM " ", works[i].delay);
    }

  printf("]\n");

  /* Create work queues */

  for (i = 0; i < nwq; i++)
    {
      wqueues[i] = work_queue_create("test", wq_priorities[i], NULL,
                                     WQUEUE_DEFAULT_STACK_SIZE,
                                     DELAY_TEST_WQUEUE_THREAD_NUM);
      if (wqueues[i] == NULL)
        {
          printf("wqueue_test: work_queue_create "
                 "failed\n");
          ret = -1;
          goto errout;
        }
    }

  /* Create threads */

  status = pthread_attr_init(&attr);
  if (status != 0)
    {
      printf("wqueue_test: pthread_attr_init "
             "failed, status=%d\n",
             status);
    }

  memset(&sparam, 0, sizeof(sparam));
  for (i = 0; i < nthreads; i++)
    {
      FAR delay_work_adder_thread_arg_t *tharg;

      sparam.sched_priority = thread_priorities[i];
      status                = pthread_attr_setschedparam(&attr, &sparam);
      if (status != 0)
        {
          printf("wqueue_test: pthread_attr_setschedparam "
                 "failed for tester, "
                 "status=%d\n",
                 status);
        }

      tharg           = &thread_work_adder_args[i];
      tharg->nthreads = nthreads;
      tharg->nworks   = n;
      tharg->nwq      = nwq;
      tharg->id       = i;
      tharg->wq       = wqueues;
      tharg->works    = works;
      status = pthread_create(&threads[i], &attr, delay_work_adder_thread,
                              tharg);
      if (status != 0)
        {
          printf("wqueue_test: pthread_create failed "
                 "for wqueue_delay_test(), "
                 "status=%d\n",
                 status);
        }
    }

  for (i = 0; i < nthreads; i++)
    {
      status = pthread_join(threads[i], NULL);

      if (status != 0)
        {
          printf("wqueue_test: pthread_join failed "
                 "for wqueue_delay_test(), "
                 "status=%d\n",
                 status);
        }
    }

  timeout_cnt = 0;
  do
    {
      usleep(100 * 1000);
      status = sem_getvalue(&finish_sem, &sval);
      if (status)
        {
          printf("wqueue_test: sem_getvalue failed "
                 "for wqueue_delay_test(), "
                 "status=%d\n",
                 status);
        }

      timeout_cnt += 100;
      printf("\rWaiting for all works to finish... timeout_cnt:%d/%d",
             timeout_cnt, timeout_ms);
      fflush(stdout);
    }
  while (timeout_cnt < timeout_ms && sval < n);

  if (timeout_cnt >= timeout_ms)
    printf("  timeout\n");
  else
    printf("  done\n");

  for (i = 0; i < n; i++)
    {
      FAR delay_work_t *work;
      clock_t actual_delay;
      uint64_t expected_ms;
      FAR char *state;
      char ext_msg[64];

      work       = &works[i];
      ext_msg[0] = '\0';

      if (work->is_valid == false)
        {
          state = "Invalid";
          invalid_count++;
        }
      else if (work->is_finish)
        {
          actual_delay = work->start_tick - work->added_tick;
          if (clock_diff(actual_delay, work->delay) < MAX_CLOCK_ERROR)
            {
              state = "Passed";
              success_count++;
            }
          else if (wq_priorities[work->wqid] < 255)
            {
              state = "Warning";
              warning_count++;
            }
          else
            {
              state = "Failed";
              fail_count++;
            }
        }
      else
        {
          actual_delay = 0;
          expected_ms  = (uint64_t)work->delay > (UINT64_MAX / 1000 - 1)
                             ? (uint64_t)work->delay / CLOCKS_PER_SEC * 1000
                             : (uint64_t)work->delay * 1000 / CLOCKS_PER_SEC;
          if (expected_ms > timeout_ms)
            {
              state = "Timeout";
              timeout_count++;
            }
          else
            {
              state = "Unexpected";
              snprintf(ext_msg, sizeof(ext_msg),
                       "%" PRIu64 " ms< %" PRIu64 "ms, but timeout",
                       expected_ms, (uint64_t)timeout_ms);
              unexpected_count++;
            }
        }

      printf("Work[%3d] in [%d] [%10s] added at %" PRIuTM
             ", qtime expected: "
             "%-10" PRIuTM " actual:%-10" PRIuTM
             ",\tdelay expected :%-6" PRIuTM " actual:%-6" PRIuTM " %s \n",
             work->id, work->wqid, state, work->added_tick, work->work.qtime,
             work->start_tick, work->delay,
             work->start_tick - work->added_tick, ext_msg);
    }

  printf("wqueue_test: delay test: "
         "Total=%d, Success=%d, Invalid=%d, Warning=%d, Failed=%d, "
         "Timeout=%d, Unexpected=%d\n",
         n, success_count, invalid_count, warning_count, fail_count,
         timeout_count, unexpected_count);

  if (fail_count > 0 || unexpected_count > 0)
    {
      printf("wqueue_test: delay test failed, has "
             "%d failed, %d unexpected\n",
             fail_count, unexpected_count);
      ret = -1;
    }

  if (invalid_count != invalid_num)
    {
      printf(
          "wqueue_test: delay test failed, expected %d invalid, found %d\n",
          invalid_num, invalid_count);
      ret = -2;
    }

errout:
  if (works != NULL)
    free(works);
  if (threads != NULL)
    free(threads);
  if (wqueues != NULL)
    {
      for (i = 0; i < nwq; i++)
        {
          if (wqueues[i] != NULL)
            work_queue_free(wqueues[i]);
        }

      free(wqueues);
    }

  if (thread_work_adder_args != NULL)
    free(thread_work_adder_args);

  return ret;
}

void wqueue_multiple_threads_and_queues_test(void)
{
  int status;
  int wq_priorities[DELAY_TEST_WQUEUE_NUM];
  int thread_priorities[DELAY_TEST_THREAD_NUM];
  int i;
  int priority = get_priority_self();

  COMPILE_TIME_ASSERT(DELAY_TEST_DELAY_MIN < DELAY_TEST_DELAY_MAX);

  printf("\nwqueue_test: multiple threads and "
         "queues test start\n");
  printf("Current thread priority: %d\n", priority);

  for (i = 0; i < DELAY_TEST_WQUEUE_NUM; i++)
    {
      wq_priorities[i] = priority + i - DELAY_TEST_WQUEUE_NUM / 2;
    }

  for (i = 0; i < DELAY_TEST_THREAD_NUM; i++)
    {
      thread_priorities[i] = priority + i - DELAY_TEST_THREAD_NUM / 2;
    }

  status = wqueue_delay_test_base(
      NULL, DELAY_TEST_NUM, 0, DELAY_TEST_DELAY_MIN, DELAY_TEST_DELAY_MAX,
      DELAY_TEST_WQUEUE_NUM, DELAY_TEST_THREAD_NUM, thread_priorities,
      wq_priorities, INT_MAX);

  if (status != 0)
    {
      wqtest_assert(false,
                    "wqueue_test: "
                    "wqueue_multiple_threads_and_queues_test() failed");
    }

  printf("wqueue_test: multiple threads and "
         "queues test done\n");
}

static void wqueue_special_delay_test(FAR const special_delay_t
                                      *special_delays)
{
  int status = 0;
  int i;
  FAR const delays_t *delays;
  int priority;

  priority = get_priority_self();

  printf("\nwqueue_test: special delay test start...\n");

  for (i = 0; i < special_delays->num; i++)
    {
      delays = &(special_delays->delays[i]);
      status = wqueue_delay_test_base(delays->data, delays->num, delays->num,
                                      DELAY_TEST_DELAY_MIN,
                                      DELAY_TEST_DELAY_MAX, 1, 1, &priority,
                                      &priority, DELAY_TEST_TIMEOUT_MS);
      if (status)
        {
          wqtest_assert(false,
                        "wqueue_test: wqueue_special_delay_test() failed");
        }
    }

  printf("wqueue_test: special delay test done.\n");
}

static void wqueue_workthread_test(int nthread)
{
  clock_t start_tick;
  int64_t exec_time;
  int efficiency;
  int threshold = WORK_THREAD_TEST_EFFICIENCY_THRESHOLD * 100;
  workthread_work_t work;

  printf("\nwqueue_test: work thread test start...\n");

  work.nthread     = nthread;
  work.nworks      = WORK_THREAD_TEST_MAX_LOOP * nthread;
  work.is_first    = true;
  atomic_set(&work.pending_cnt, 0);
  atomic_set(&work.total_cnt, 0);
  atomic_set(&work.finish_cnt, 0);
  atomic_set(&work.total_ticks, 0);
  sem_init(&work.finish_sem, 0, 0);
  work.wq = work_queue_create("test", WQUEUE_DEFAULT_PRIORITY, NULL,
                              WQUEUE_DEFAULT_STACK_SIZE, nthread);
  if (work.wq == NULL)
    {
      printf("wqueue_test: work_queue_create failed\n");
      return;
    }

  work.works = zalloc((work.nworks + 1) * sizeof(struct work_s));
  if (!work.works)
    {
      printf("Memory allocation failed\n");
      goto errout;
    }

  start_tick = clock_systime_ticks();
  workthread_worker(&work);
  sem_wait(&work.finish_sem);
  exec_time  = clock_systime_ticks() - start_tick;
  efficiency = atomic_read(&work.total_ticks) * 10000ull /
                 (exec_time * nthread);
  printf("Thread Num: %d;\
         Efficiency: %2d.%02d%%; \
         Total time: %" PRIu32 " ticks, \
         Execution: %" PRIu64 " ticks\n",
         nthread, efficiency / 100, efficiency % 100,
         (uint32_t)atomic_read(&work.total_ticks), exec_time);

  wqtest_assert(efficiency > threshold,
                "wqueue_test: work thread test failed, "
                "multiple threads efficiency too low");

  printf("wqueue_test: work thread test done\n");

errout:
  work_queue_free(work.wq);
}

#ifdef CONFIG_WQUEUE_NOTIFIER
static void test_notifier1_callback(FAR void *arg)
{
  FAR struct test_work1_s *test_work = (FAR struct test_work1_s *)arg;
  printf("notifier_callback: triggered.\n");
  fflush(stdout);
  if (test_work)
    {
      test_work->completed = true;
      printf("test_work1 completed.\n");
      printf("start triggering notifier signal 2.\n");
      work_notifier_signal(WORK_NET_DOWN, (FAR void *)(intptr_t)FD2);
    }
}

static void test_notifier2_callback(FAR void *arg)
{
  FAR struct test_work1_s *test_work = (struct test_work1_s *)arg;
  test_work->completed               = true;
  printf("notifier callback2 triggered.\n");
}

static int create_and_add_notifier(FAR void *test_work,
                                   int evtype,
                                   int fd,
                                   worker_t worker)
{
  struct work_notifier_s info;
  memset(&info, 0, sizeof(struct work_notifier_s));
  info.evtype    = evtype;
  info.qid       = HPWORK;
  info.qualifier = (void *)(intptr_t)fd;
  info.arg       = test_work;
  info.worker    = worker;

  int ret = work_notifier_setup(&info);
  wqtest_assert(ret != -ENOMEM, "wqueue_test: work_notifier_setup() failed");

  return ret;
}

static void test_work_notifier_signal(void)
{
  struct test_state_s test_state;
  int notifier_key1;
  int notifier_key2;

  memset(&test_state.test_work1, 0, sizeof(test_state.test_work1));
  memset(&test_state.test_work2, 0, sizeof(test_state.test_work2));

  notifier_key1 = create_and_add_notifier(
      &test_state.test_work1, WORK_NET_DOWN, FD1, test_notifier1_callback);
  wqtest_assert(notifier_key1 != -ENOMEM,
                "wqueue_test: work_notifier_setup() failed");

  notifier_key2 = create_and_add_notifier(
      &test_state.test_work2, WORK_NET_DOWN, FD2, test_notifier2_callback);
  wqtest_assert(notifier_key2 != -ENOMEM,
                "wqueue_test: work_notifier_setup() failed");

  printf("start sending notifier signal 1.\n");
  work_notifier_signal(WORK_NET_DOWN, (void *)(intptr_t)FD1);

  while (!test_state.test_work1.completed)
    {
      usleep(1000);
    }

  wqtest_assert(test_state.test_work1.completed, NULL);

  if (test_state.test_work1.completed)
    {
      printf("notifier 1 processed success.\n");
    }
  else
    {
      printf("notifier 1 processing failed.\n");
    }

  while (!test_state.test_work2.completed)
    {
      usleep(1000);
    }

  wqtest_assert(test_state.test_work2.completed, NULL);

  if (test_state.test_work2.completed)
    {
      printf("notifier 2 processed success.\n");
    }
  else
    {
      printf("notifier 2 processing failed.\n");
    }

  work_notifier_teardown(notifier_key1);
  work_notifier_teardown(notifier_key2);
}

static void test_notifier_callback2(FAR void *arg)
{
  FAR struct test_work2_s *test_work = (struct test_work2_s *)arg;
  if (test_work->first_notify)
    {
      printf("notifier callback2 triggered:first notify.\n");
      test_work->first_notify = false;
    }
  else
    {
      printf("notifier callback2 triggered:second notify.\n");
      test_work->completed = true;
    }
}

static void test_work_notifier_teardown(void)
{
  struct test_work2_s test_work;
  int notifier_key;
  int fd                 = 1;
  unsigned int elapsed   = 0;
  test_work.first_notify = true;

  memset(&test_work, 0, sizeof(test_work));

  notifier_key = create_and_add_notifier(&test_work, WORK_NET_DOWN, fd,
                                         test_notifier_callback2);

  printf("start sending notifier signal.\n");
  work_notifier_signal(WORK_NET_DOWN, (FAR void *)(intptr_t)(fd));

  printf("start sending teardown notifier.\n");
  work_notifier_teardown(notifier_key);
  printf("notifier successfully torn down on first attempt.\n");

  test_work.completed = false;
  printf("sending second notifier signal.\n");
  work_notifier_signal(WORK_NET_DOWN, (FAR void *)(intptr_t)fd);

  while (!test_work.completed && elapsed < WORK_NOTIFIER_TEST_TIMEOUT_US)
    {
      usleep(1000);
      elapsed += 1000;
    }

  printf("test completed,work status:%s\n",
         test_work.completed ? "completed" : "teardown");
}
#endif /* CONFIG_WQUEUE_NOTIFIER */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void wqueue_test(void)
{
  FAR void *wq;
  int i;

#ifdef CONFIG_SCHED_HPWORK
  printf("wqueue_test: HPWORK\n");
  wqueue_priority_test(HPWORK, NULL, CONFIG_SCHED_HPWORKPRIORITY);
  printf("wqueue_test: HPWORK done\n");
#endif

#ifdef CONFIG_SCHED_LPWORK
  printf("wqueue_test: LPWORK\n");
  wqueue_priority_test(LPWORK, NULL, CONFIG_SCHED_LPWORKPRIORITY);
  printf("wqueue_test: HPWORK done\n");
#endif

  for (i = 1; i < 3; i++)
    {
      printf("wqueue_test: test %d\n", i);
      wq = work_queue_create("test", WQUEUE_DEFAULT_PRIORITY, NULL,
                             WQUEUE_DEFAULT_STACK_SIZE, i);
      DEBUGASSERT(wq != NULL);
      wqueue_priority_test(0, wq, WQUEUE_DEFAULT_PRIORITY);
      work_queue_free(wq);
      printf("wqueue_test: test %d done\n", i);
    }

  wqueue_period_and_cancel_test();
  wqueue_recursive_test();
  wqueue_multiple_threads_and_queues_test();
  wqueue_special_delay_test(&special_delays_test_arg);
  wqueue_workthread_test(WORK_THREAD_TEST_THREAD_NUM);

#ifdef CONFIG_WQUEUE_NOTIFIER
  printf("\nwqueue_test: work notifier test start...\n");
  test_work_notifier_signal();
  test_work_notifier_teardown();
  printf("wqueue_test: work notifier test done.\n");
#endif
}

#endif /* CONFIG_SCHED_WORKQUEUE */
