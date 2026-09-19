/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_event_test.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>

#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/kthread.h>
#include <nuttx/sched.h>
#include <nuttx/semaphore.h>
#include <nuttx/signal.h>
#include <nuttx/spinlock.h>

#include "esp32s31_wifi_event.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* One boot-time test, no user data or pin access.  Keep job storage static
 * so an unexpected task-delete failure cannot invalidate a worker's data.
 */

static sem_t g_start;
static sem_t g_done;
static sem_t g_park;
static void *g_event;
static uint32_t g_ticks;
static uint32_t g_result;
static int g_worker_cpu;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int event_worker(int argc, char *argv[])
{
  int ret = nxsem_wait_uninterruptible(&g_start);

  if (ret >= 0)
    {
      g_worker_cpu = sched_getcpu();
      g_result = esp32s31_wifi_event_wait(g_event, 3, 1, 1, g_ticks);
      nxsem_post(&g_done);
    }

  /* Remain alive until the parent deletes this exact PID.  Do not race an
   * automatically reaped thread or a subsequently reused PID in cleanup.
   */

  nxsem_wait_uninterruptible(&g_park);
  return 0;
}

static int wait_blocked(pid_t pid)
{
  struct tcb_s *tcb;
  irqstate_t flags;
  bool blocked;
  int attempt;

  for (attempt = 0; attempt < 1000; attempt++)
    {
      tcb = nxsched_get_tcb(pid);
      if (tcb == NULL)
        {
          return -ESRCH;
        }

      flags = enter_critical_section();
      blocked = tcb->task_state == TSTATE_WAIT_SEM &&
                tcb->waitobj != &g_start && tcb->waitobj != &g_park;
      leave_critical_section(flags);
      nxsched_put_tcb(tcb);
      if (blocked && esp32s31_wifi_event_waiter_count() == 1)
        {
          return 0;
        }

      nxsig_usleep(1000);
    }

  return -ETIMEDOUT;
}

static int event_case(int parent_cpu, int mode)
{
  cpu_set_t mask;
  uint32_t expected = mode == 0 ? 0x83 : mode == 1 ? 0x81 : 0;
  int initialized = 0;
  int ret;
  pid_t pid = -1;

  g_event = esp32s31_wifi_event_create();
  if (g_event == NULL)
    {
      return -ENOMEM;
    }

  ret = nxsem_init(&g_start, 0, 0);
  if (ret < 0)
    {
      goto out;
    }

  initialized++;
  ret = nxsem_init(&g_done, 0, 0);
  if (ret < 0)
    {
      goto out;
    }

  initialized++;
  ret = nxsem_init(&g_park, 0, 0);
  if (ret < 0)
    {
      goto out;
    }

  initialized++;
  g_result = UINT32_MAX;
  g_worker_cpu = -1;
  g_ticks = mode == 1 ? 2 : UINT32_MAX;
  esp32s31_wifi_event_set(g_event, mode == 1 ? 0x81 : 0x80);
  pid = kthread_create("s31-event", 100, 3072, event_worker, NULL);
  if (pid < 0)
    {
      ret = pid;
      goto out;
    }

  CPU_ZERO(&mask);
  CPU_SET(1 - parent_cpu, &mask);
  ret = nxsched_set_affinity(pid, sizeof(mask), &mask);
  if (ret < 0)
    {
      goto out;
    }

  nxsem_post(&g_start);
  if (mode != 1)
    {
      ret = wait_blocked(pid);
      if (ret < 0)
        {
          goto out;
        }
    }

  if (mode == 0)
    {
      esp32s31_wifi_event_set(g_event, 3);
    }
  else if (mode == 2)
    {
      esp32s31_wifi_event_delete(g_event);
      g_event = NULL;
    }
  else if (mode == 3)
    {
      ret = kthread_delete(pid);
      if (ret < 0)
        {
          goto out;
        }

      pid = -1;
    }

  if (mode != 3)
    {
      ret = nxsem_tickwait_uninterruptible(&g_done, SEC2TICK(2));
      if (ret < 0)
        {
          goto out;
        }
    }

  if (g_worker_cpu != 1 - parent_cpu || sched_getcpu() != parent_cpu ||
      (mode != 3 && g_result != expected) ||
      esp32s31_wifi_event_waiter_count() != 0)
    {
      ret = -EIO;
      goto out;
    }

  if (g_event != NULL &&
      esp32s31_wifi_event_clear(g_event, 0) != (mode == 1 ? 0x81 : 0x80))
    {
      ret = -EIO;
      goto out;
    }

  ret = 0;
out:
  if (pid > 0 && kthread_delete(pid) < 0)
    {
      /* Leave resources valid for the still-live worker.  Stop this boot
       * test; host recovery resets/reflashes rather than reusing the job.
       */

      syslog(LOG_ERR, "EVENT_TEST cleanup failed pid=%d\n", pid);
      return -EBUSY;
    }

  if (g_event != NULL)
    {
      esp32s31_wifi_event_delete(g_event);
      g_event = NULL;
    }

  if (initialized >= 3)
    {
      nxsem_destroy(&g_park);
    }

  if (initialized >= 2)
    {
      nxsem_destroy(&g_done);
    }

  if (initialized >= 1)
    {
      nxsem_destroy(&g_start);
    }

  syslog(LOG_INFO, "EVENT_TEST parent=%d worker=%d mode=%d ret=%d\n",
         parent_cpu, g_worker_cpu, mode, ret);
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int esp32s31_event_test(void)
{
  cpu_set_t saved;
  cpu_set_t mask;
  int cpu;
  int mode;
  int ret;
  int restore;

  ret = nxsched_get_affinity(0, sizeof(saved), &saved);
  if (ret < 0)
    {
      return ret;
    }

  for (cpu = 0; cpu < 2; cpu++)
    {
      CPU_ZERO(&mask);
      CPU_SET(cpu, &mask);
      ret = nxsched_set_affinity(0, sizeof(mask), &mask);
      if (ret < 0)
        {
          break;
        }

      for (mode = 0; mode < 4; mode++)
        {
          ret = event_case(cpu, mode);
          if (ret < 0)
            {
              break;
            }
        }

      if (ret < 0)
        {
          break;
        }
    }

  restore = nxsched_set_affinity(0, sizeof(saved), &saved);
  if (ret == 0)
    {
      ret = restore;
    }

  syslog(LOG_INFO, "EVENT_TEST=%s cases=8 ret=%d\n",
         ret == 0 ? "PASS" : "FAIL", ret);
  return ret;
}
