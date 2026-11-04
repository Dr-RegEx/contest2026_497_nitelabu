/****************************************************************************
 * sched/init/nx_kthread_gc.c
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

#include <nuttx/config.h>

#include <assert.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/kthread.h>
#include <nuttx/mm/mm.h>
#include <nuttx/signal.h>

#include "init/init.h"

/* A normal schedulable thread is necessary: freeing may acquire heap locks.
 * Visiting a CPU through the scheduler also ensures its previous outgoing
 * thread has switched off any stack placed on that CPU's deferred list.
 * Interrupt callbacks and a remote list sweep do not provide this contract.
 */

static int nx_kthread_gc(int argc, FAR char *argv[])
{
  cpu_set_t cpuset;
  int cpu;
  int ret;

  DEBUGASSERT(argc == 2 && argv[1] != NULL);
  cpu = atoi(argv[1]);
  DEBUGASSERT(cpu >= 0 && cpu < CONFIG_SMP_NCPUS);
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  ret = nxsched_set_affinity(0, sizeof(cpuset), &cpuset);
  if (ret < 0)
    {
      serr("ERROR: Kernel collector affinity cpu=%d: %d\n", cpu, ret);
      PANIC();
    }

  for (; ; )
    {
      DEBUGASSERT(this_cpu() == cpu);
      kmm_collect();
      nxsig_usleep(CONFIG_SCHED_KTHREAD_GC_INTERVAL_USEC);
    }

  return 0;
}

void nx_kthread_gc_start(void)
{
  char cpuid[12];
  FAR char *argv[] = {cpuid, NULL};
  int cpu;
  int pid;

  for (cpu = 0; cpu < CONFIG_SMP_NCPUS; cpu++)
    {
      /* kthread_create copies arguments into the new thread's stack before
       * returning, so this local buffer can be reused for the next CPU.
       */

      snprintf(cpuid, sizeof(cpuid), "%d", cpu);
      pid = kthread_create("kthread-gc", CONFIG_SCHED_KTHREAD_GC_PRIORITY,
                           CONFIG_SCHED_KTHREAD_GC_STACKSIZE,
                           nx_kthread_gc, argv);
      if (pid < 0)
        {
          serr("ERROR: Kernel collector creation cpu=%d: %d\n", cpu, pid);
          PANIC();
        }
    }
}
