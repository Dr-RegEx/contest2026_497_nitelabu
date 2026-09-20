/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_mqueue_test.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sched.h>
#include <stdint.h>
#include <syslog.h>

#include <nuttx/clock.h>
#include <nuttx/fs/fs.h>
#include <nuttx/mqueue.h>
#include <nuttx/sched.h>

#define QUEUE_TEST_NAME   "/s31-queue-boot-test"
#define QUEUE_TEST_ROUNDS 64

static int queue_case(int cpu)
{
  struct mq_attr attr = {0};
  struct file mq = {0};
  uint32_t sent = 0x53101234;
  uint32_t received = 0;
  unsigned int priority;
  clock_t start;
  clock_t elapsed = 0;
  int ret;
  int cleanup;
  int index;

  attr.mq_maxmsg = 1;
  attr.mq_msgsize = sizeof(sent);
  ret = file_mq_open(&mq, QUEUE_TEST_NAME, O_RDWR | O_CREAT | O_EXCL,
                     0600, &attr);
  if (ret < 0)
    {
      return ret;
    }

  ret = file_mq_ticksend(&mq, (const char *)&sent, sizeof(sent), 0, 0);
  if (ret < 0)
    {
      goto out;
    }

  start = clock_systime_ticks();
  for (index = 0; index < QUEUE_TEST_ROUNDS; index++)
    {
      ret = file_mq_ticksend(&mq, (const char *)&sent, sizeof(sent), 0, 0);
      if (ret != -ETIMEDOUT)
        {
          ret = -EIO;
          goto out;
        }
    }

  elapsed = clock_systime_ticks() - start;

  /* Allow scheduling noise, but reject one watchdog tick per failed send.
   * This is a bounded boot diagnostic, not a hard real-time latency proof.
   */

  if (elapsed >= QUEUE_TEST_ROUNDS / 4 || sched_getcpu() != cpu)
    {
      ret = -ETIMEDOUT;
      goto out;
    }

  ret = file_mq_getattr(&mq, &attr);
  if (ret < 0 || attr.mq_curmsgs != 1)
    {
      ret = -EIO;
      goto out;
    }

  /* A positive timeout must still expire while the queue remains full. */

  start = clock_systime_ticks();
  ret = file_mq_ticksend(&mq, (const char *)&sent, sizeof(sent), 0, 2);
  if (ret != -ETIMEDOUT || clock_systime_ticks() - start < 2)
    {
      ret = -EIO;
      goto out;
    }

  attr.mq_flags = O_NONBLOCK;
  ret = file_mq_setattr(&mq, &attr, NULL);
  if (ret < 0)
    {
      goto out;
    }

  ret = file_mq_ticksend(&mq, (const char *)&sent, sizeof(sent), 0, 0);
  if (ret != -EAGAIN)
    {
      ret = -EIO;
      goto out;
    }

  ret = file_mq_receive(&mq, (char *)&received, sizeof(received), &priority);
  if (ret != sizeof(received) || received != sent || priority != 0)
    {
      ret = -EIO;
      goto out;
    }

  ret = file_mq_getattr(&mq, &attr);
  if (ret == 0 && attr.mq_curmsgs != 0)
    {
      ret = -EIO;
    }

out:
  cleanup = file_mq_close(&mq);
  if (ret == 0)
    {
      ret = cleanup;
    }

  cleanup = file_mq_unlink(QUEUE_TEST_NAME);
  if (ret == 0)
    {
      ret = cleanup;
    }

  syslog(LOG_INFO, "MQUEUE_CASE cpu=%d rounds=%d elapsed=%lu ret=%d\n",
         cpu, QUEUE_TEST_ROUNDS, (unsigned long)elapsed, ret);
  return ret;
}

int esp32s31_mqueue_test(void)
{
  cpu_set_t saved;
  cpu_set_t mask;
  int ret;
  int restore;
  int cpu;

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
      if (ret == 0)
        {
          ret = queue_case(cpu);
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

  syslog(LOG_INFO, "MQUEUE_TEST=%s cases=2 ret=%d\n",
         ret == 0 ? "PASS" : "FAIL", ret);
  return ret;
}
