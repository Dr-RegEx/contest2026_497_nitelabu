/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <nuttx/kthread.h>
#include <nuttx/mutex.h>
#include <nuttx/sched.h>
#include <nuttx/semaphore.h>
#include <errno.h>
#include <sched.h>

#include "esp32s31_ble_dispatch.h"

static mutex_t g_dispatch_lock = NXMUTEX_INITIALIZER;
static sem_t g_request = SEM_INITIALIZER(0);
static sem_t g_complete = SEM_INITIALIZER(0);
static int (*g_function)(void *);
static void *g_argument;
static int g_result;
static pid_t g_worker = -1;

static int ble_worker(int argc, char **argv)
{
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  g_result = nxsched_set_affinity(0, sizeof(cpuset), &cpuset);
  nxsem_post(&g_complete);
  if (g_result < 0)
    {
      return g_result;
    }

  for (;;)
    {
      nxsem_wait_uninterruptible(&g_request);
      g_result = g_function(g_argument);
      nxsem_post(&g_complete);
    }

  return 0;
}

int esp32s31_ble_dispatch_init(void)
{
  int ret = nxmutex_lock(&g_dispatch_lock);

  if (ret < 0)
    {
      return ret;
    }

  if (g_worker < 0)
    {
      nxsem_set_protocol(&g_request, SEM_PRIO_NONE);
      nxsem_set_protocol(&g_complete, SEM_PRIO_NONE);
      g_worker = kthread_create("ble_ctrl", 124, 4096, ble_worker, NULL);
      if (g_worker < 0)
        {
          ret = g_worker;
        }
      else
        {
          nxsem_wait_uninterruptible(&g_complete);
          ret = g_result;
          if (ret < 0)
            {
              g_worker = -1;
            }
        }
    }

  nxmutex_unlock(&g_dispatch_lock);
  return ret;
}

int esp32s31_ble_dispatch(int (*function)(void *), void *argument)
{
  int ret = nxmutex_lock(&g_dispatch_lock);

  if (ret < 0)
    {
      return ret;
    }

  if (g_worker < 0 || function == NULL)
    {
      nxmutex_unlock(&g_dispatch_lock);
      return -ENODEV;
    }

  g_function = function;
  g_argument = argument;
  nxsem_post(&g_request);
  nxsem_wait_uninterruptible(&g_complete);
  ret = g_result;
  nxmutex_unlock(&g_dispatch_lock);
  return ret;
}
