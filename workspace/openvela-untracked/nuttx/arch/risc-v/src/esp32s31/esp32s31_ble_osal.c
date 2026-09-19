/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_ble_osal.c
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/kthread.h>
#include <nuttx/mutex.h>
#include <nuttx/sched.h>
#include <nuttx/semaphore.h>
#include <nuttx/spinlock.h>
#include <nuttx/wdog.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "btdm_osal.h"
#include "esp_attr.h"
#include "esp_irq.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_rom_sys.h"

#if defined(CONFIG_ESPRESSIF_SPIRAM_USER_HEAP) || \
    (!defined(CONFIG_BUILD_FLAT) && !defined(CONFIG_BUILD_KERNEL))
#  error "S31 BLE OSAL requires internal kernel/controller memory"
#endif

/* Controller-owned tasks and IRQs must be stopped before pool destruction.
 * Intrusive queues avoid allocating in controller interrupt handlers.  OSAL
 * time is milliseconds, independent of the NuttX scheduler tick frequency.
 */

struct s31_queue_s;
struct s31_event_s
{
  struct s31_event_s *next;
  struct s31_queue_s *queue;
  struct btdm_osal_event *owner;
  btdm_osal_event_fn *fn;
  void *arg;
};

struct s31_queue_s
{
  struct s31_event_s *head;
  struct s31_event_s *tail;
  sem_t ready;
};

struct s31_callout_s
{
  struct wdog_s wd;
  struct btdm_osal_event event;
  struct btdm_osal_eventq *queue;
  uint32_t expiry;
};

struct s31_pool_s
{
  void *storage;
  uint8_t *used;
  size_t size;
  unsigned int count;
};

struct s31_intr_s
{
  btdm_osal_intr_fn *fn;
  void *arg;
  int source;
  int cpuint;
};

enum s31_pool_id_e { EVENTS, QUEUES, CALLOUTS, SEMAPHORES, MUTEXES, NPOOLS };
static struct s31_pool_s g_pools[NPOOLS];
static struct btdm_osal_eventq g_timerq;
static rmutex_t g_timer_gate = NXRMUTEX_INITIALIZER;
static pid_t g_timer_pid = -1;
static unsigned int g_critical_nest;
static irqstate_t g_critical_state[32];

static void *IRAM_ATTR s31_pool_get(enum s31_pool_id_e id, void *old)
{
  struct s31_pool_s *p = &g_pools[id];
  irqstate_t flags = enter_critical_section();
  unsigned int i;
  for (i = 0; i < p->count; i++)
    {
      void *slot = (char *)p->storage + p->size * i;
      if (old == slot && p->used[i])
        {
          leave_critical_section(flags);
          return slot;
        }
    }

  for (i = 0; i < p->count; i++)
    {
      if (!p->used[i])
        {
          void *slot = (char *)p->storage + p->size * i;
          p->used[i] = 1;
          memset(slot, 0, p->size);
          leave_critical_section(flags);
          return slot;
        }
    }

  leave_critical_section(flags);
  return NULL;
}

static void IRAM_ATTR s31_pool_put(enum s31_pool_id_e id, void *slot)
{
  struct s31_pool_s *p = &g_pools[id];
  size_t index = ((uintptr_t)slot - (uintptr_t)p->storage) / p->size;
  irqstate_t flags = enter_critical_section();
  DEBUGASSERT(index < p->count && p->used[index]);
  p->used[index] = 0;
  leave_critical_section(flags);
}

static int IRAM_ATTR s31_wait(sem_t *sem, uint32_t ms)
{
  if (up_interrupt_context())
    {
      /* The pinned OSAL permits a nonblocking take from ISR context.
       * nxsem_trywait() explicitly rejects ISR callers in this NuttX
       * version. The slow primitive handles a counting SEM_PRIO_NONE
       * semaphore under its own critical section without blocking or
       * assigning mutex ownership. Every OSAL semaphore uses that mode.
       */

      return ms == 0 ? nxsem_trywait_slow(sem) : -EPERM;
    }

  if (ms == 0)
    {
      return nxsem_trywait(sem);
    }

  if (ms == UINT32_MAX)
    {
      return nxsem_wait_uninterruptible(sem);
    }

  return nxsem_tickwait_uninterruptible(sem,
                                        MAX(1, MSEC2TICK((uint64_t)ms)));
}

static int IRAM_ATTR s31_result(int ret)
{
  if (ret >= 0)
    {
      return BTDM_OSAL_OK;
    }

  if (ret == -ETIMEDOUT || ret == -EAGAIN || ret == -EBUSY)
    {
      return BTDM_OSAL_TIMEOUT;
    }

  return ret == -EPERM ? BTDM_OSAL_ERR_IN_ISR : BTDM_OSAL_ERROR;
}

void IRAM_ATTR wr_btdm_osal_eventq_remove(struct btdm_osal_eventq *evq,
                                        struct btdm_osal_event *ev)
{
  struct s31_queue_s *q = evq->eventq;
  struct s31_event_s *e = ev->event;
  struct s31_event_s **link;
  struct s31_event_s *previous = NULL;
  irqstate_t flags = enter_critical_section();
  if (e && e->queue == q)
    {
      for (link = &q->head; *link && *link != e; link = &(*link)->next)
        {
          previous = *link;
        }

      DEBUGASSERT(*link == e);
      *link = e->next;
      if (q->tail == e)
        {
          q->tail = previous;
        }

      e->queue = NULL;
      e->next = NULL;
    }

  leave_critical_section(flags);
}

static void IRAM_ATTR s31_put(struct btdm_osal_eventq *evq,
                             struct btdm_osal_event *ev, bool front)
{
  struct s31_queue_s *q = evq->eventq;
  struct s31_event_s *e = ev->event;
  irqstate_t flags = enter_critical_section();
  DEBUGASSERT(q && e);
  if (!e->queue)
    {
      bool empty = q->head == NULL;
      e->queue = q;
      if (front)
        {
          e->next = q->head;
          q->head = e;
        }
      else
        {
          e->next = NULL;
          if (q->tail)
            {
              q->tail->next = e;
            }
          else
            {
              q->head = e;
            }
        }

      if (empty || !front)
        {
          q->tail = e;
        }

      if (empty)
        {
          int value;
          nxsem_get_value(&q->ready, &value);
          if (value <= 0)
            {
              nxsem_post(&q->ready);
            }
        }
    }

  leave_critical_section(flags);
}

void IRAM_ATTR wr_btdm_osal_eventq_put(struct btdm_osal_eventq *q,
                                     struct btdm_osal_event *e)
{
  s31_put(q, e, false);
}

void IRAM_ATTR wr_btdm_osal_eventq_put_to_front(struct btdm_osal_eventq *q,
                                              struct btdm_osal_event *e)
{
  s31_put(q, e, true);
}

bool IRAM_ATTR wr_btdm_osal_eventq_is_empty(struct btdm_osal_eventq *evq)
{
  struct s31_queue_s *q = evq->eventq;
  irqstate_t flags = enter_critical_section();
  bool empty = q->head == NULL;
  leave_critical_section(flags);
  return empty;
}

struct btdm_osal_event *IRAM_ATTR
wr_btdm_osal_eventq_get(struct btdm_osal_eventq *evq, uint32_t timeout)
{
  struct s31_queue_s *q = evq->eventq;
  uint32_t start = wr_btdm_osal_time_get();
  for (;;)
    {
      irqstate_t flags = enter_critical_section();
      struct s31_event_s *e = q->head;
      if (e)
        {
          q->head = e->next;
          if (!q->head)
            {
              q->tail = NULL;
            }

          e->queue = NULL;
          e->next = NULL;
          leave_critical_section(flags);
          return e->owner;
        }

      leave_critical_section(flags);
      if (timeout != UINT32_MAX)
        {
          uint32_t elapsed = wr_btdm_osal_time_get() - start;
          if (elapsed >= timeout || s31_wait(&q->ready,
                                             timeout - elapsed) < 0)
            {
              return NULL;
            }
        }
      else if (s31_wait(&q->ready, timeout) < 0)
        {
          return NULL;
        }
    }
}

void wr_btdm_osal_eventq_init(struct btdm_osal_eventq *evq)
{
  struct s31_queue_s *old = evq->eventq;
  struct s31_queue_s *q = s31_pool_get(QUEUES, old);
  ASSERT(q);
  if (q != old)
    {
      nxsem_init(&q->ready, 0, 0);
      nxsem_set_protocol(&q->ready, SEM_PRIO_NONE);
    }

  evq->eventq = q;
  while (q->head)
    {
      wr_btdm_osal_eventq_remove(evq, q->head->owner);
    }

  while (nxsem_trywait(&q->ready) == 0)
    {
    }
}

void wr_btdm_osal_eventq_deinit(struct btdm_osal_eventq *evq)
{
  struct s31_queue_s *q = evq->eventq;
  if (q)
    {
      while (q->head)
        {
          wr_btdm_osal_eventq_remove(evq, q->head->owner);
        }

      nxsem_destroy(&q->ready);
      s31_pool_put(QUEUES, q);
      evq->eventq = NULL;
    }
}

void IRAM_ATTR wr_btdm_osal_event_reset(struct btdm_osal_event *ev)
{
  struct s31_event_s *e = ev->event;
  if (e && e->queue)
    {
      struct btdm_osal_eventq q = { .eventq = e->queue };
      wr_btdm_osal_eventq_remove(&q, ev);
    }
}

void IRAM_ATTR wr_btdm_osal_event_init(struct btdm_osal_event *ev,
                           btdm_osal_event_fn *fn, void *arg)
{
  struct s31_event_s *e = s31_pool_get(EVENTS, ev->event);
  ASSERT(e && fn);
  ev->event = e;
  wr_btdm_osal_event_reset(ev);
  e->owner = ev;
  e->fn = fn;
  e->arg = arg;
}

void IRAM_ATTR wr_btdm_osal_event_deinit(struct btdm_osal_event *ev)
{
  if (ev->event)
    {
      wr_btdm_osal_event_reset(ev);
      s31_pool_put(EVENTS, ev->event);
      ev->event = NULL;
    }
}

void IRAM_ATTR wr_btdm_osal_event_run(struct btdm_osal_event *ev)
{
  struct s31_event_s *e = ev->event;
  btdm_osal_event_fn *fn = e->fn;
  fn(ev);
}

bool IRAM_ATTR wr_btdm_osal_event_is_queued(struct btdm_osal_event *ev)
{
  struct s31_event_s *e = ev->event;
  return e && e->queue != NULL;
}

void *IRAM_ATTR wr_btdm_osal_event_get_arg(struct btdm_osal_event *ev)
{
  return ((struct s31_event_s *)ev->event)->arg;
}

void IRAM_ATTR wr_btdm_osal_event_set_arg(struct btdm_osal_event *ev, void *arg)
{
  ((struct s31_event_s *)ev->event)->arg = arg;
}

int wr_btdm_osal_mutex_init(struct btdm_osal_mutex *mu)
{
  rmutex_t *m = s31_pool_get(MUTEXES, mu->mutex);
  if (!m)
    {
      return BTDM_OSAL_ENOMEM;
    }

  if (m != mu->mutex)
    {
      nxrmutex_init(m);
    }

  mu->mutex = m;
  return BTDM_OSAL_OK;
}

int wr_btdm_osal_mutex_deinit(struct btdm_osal_mutex *mu)
{
  if (!mu->mutex)
    {
      return BTDM_OSAL_INVALID_PARM;
    }

  int ret = nxrmutex_destroy(mu->mutex);
  if (ret < 0)
    {
      return s31_result(ret);
    }

  s31_pool_put(MUTEXES, mu->mutex);
  mu->mutex = NULL;
  return BTDM_OSAL_OK;
}

int wr_btdm_osal_mutex_pend(struct btdm_osal_mutex *mu, uint32_t timeout)
{
  if (up_interrupt_context())
    {
      return BTDM_OSAL_ERR_IN_ISR;
    }

  if (!mu->mutex)
    {
      return BTDM_OSAL_INVALID_PARM;
    }

  return s31_result(timeout == UINT32_MAX ? nxrmutex_lock(mu->mutex) :
                    timeout == 0 ? nxrmutex_trylock(mu->mutex) :
                    nxrmutex_timedlock(mu->mutex, timeout));
}

int wr_btdm_osal_mutex_release(struct btdm_osal_mutex *mu)
{
  if (up_interrupt_context())
    {
      return BTDM_OSAL_ERR_IN_ISR;
    }

  return mu->mutex ? s31_result(nxrmutex_unlock(mu->mutex)) :
                     BTDM_OSAL_INVALID_PARM;
}

int wr_btdm_osal_sem_init(struct btdm_osal_sem *sem, uint16_t tokens)
{
  sem_t *s;
  if (tokens > 128)
    {
      return BTDM_OSAL_INVALID_PARM;
    }

  s = s31_pool_get(SEMAPHORES, sem->sem);
  if (!s)
    {
      return BTDM_OSAL_ENOMEM;
    }

  if (s != sem->sem)
    {
      nxsem_init(s, 0, tokens);
      nxsem_set_protocol(s, SEM_PRIO_NONE);
    }

  sem->sem = s;
  return BTDM_OSAL_OK;
}

int wr_btdm_osal_sem_deinit(struct btdm_osal_sem *sem)
{
  if (!sem->sem)
    {
      return BTDM_OSAL_INVALID_PARM;
    }

  int ret = nxsem_destroy(sem->sem);
  if (ret < 0)
    {
      return s31_result(ret);
    }

  s31_pool_put(SEMAPHORES, sem->sem);
  sem->sem = NULL;
  return BTDM_OSAL_OK;
}

int IRAM_ATTR wr_btdm_osal_sem_pend(struct btdm_osal_sem *sem,
                                   uint32_t timeout)
{
  return sem->sem ? s31_result(s31_wait(sem->sem, timeout)) :
                    BTDM_OSAL_INVALID_PARM;
}

int IRAM_ATTR wr_btdm_osal_sem_release(struct btdm_osal_sem *sem)
{
  int value;
  int ret;
  irqstate_t flags;
  if (!sem->sem)
    {
      return BTDM_OSAL_INVALID_PARM;
    }

  flags = enter_critical_section();
  nxsem_get_value(sem->sem, &value);
  ret = value >= 128 ? -EOVERFLOW : nxsem_post(sem->sem);
  leave_critical_section(flags);
  return s31_result(ret);
}

uint16_t IRAM_ATTR wr_btdm_osal_sem_get_count(struct btdm_osal_sem *sem)
{
  int value = 0;
  if (sem->sem)
    {
      nxsem_get_value(sem->sem, &value);
    }

  return MAX(0, value);
}

static void IRAM_ATTR s31_expire(wdparm_t arg)
{
  struct s31_callout_s *c = (void *)(uintptr_t)arg;
  wr_btdm_osal_eventq_put(c->queue, &c->event);
}

void IRAM_ATTR wr_btdm_osal_callout_stop(struct btdm_osal_callout *co)
{
  struct s31_callout_s *c = co->co;
  irqstate_t flags = enter_critical_section();
  if (c)
    {
      wd_cancel(&c->wd);
      wr_btdm_osal_event_reset(&c->event);
    }

  leave_critical_section(flags);
}

int wr_btdm_osal_callout_init(struct btdm_osal_callout *co,
                            struct btdm_osal_eventq *evq,
                            btdm_osal_event_fn *fn, void *arg)
{
  struct s31_callout_s *c = s31_pool_get(CALLOUTS, co->co);
  if (!c)
    {
      return -ENOMEM;
    }

  co->co = c;
  wr_btdm_osal_callout_stop(co);
  c->queue = evq ? evq : &g_timerq;
  wr_btdm_osal_event_init(&c->event, fn, arg);
  return 0;
}

void wr_btdm_osal_callout_deinit(struct btdm_osal_callout *co)
{
  /* The gate serializes direct callbacks with destruction, including a
   * callback deleting itself. Controller event queues require their own
   * consumer to be quiescent before destroying a dispatched event.
   */

  DEBUGASSERT(!up_interrupt_context());
  nxrmutex_lock(&g_timer_gate);
  if (co->co)
    {
      struct s31_callout_s *c = co->co;
      wr_btdm_osal_callout_stop(co);
      wr_btdm_osal_event_deinit(&c->event);
      s31_pool_put(CALLOUTS, c);
      co->co = NULL;
    }

  nxrmutex_unlock(&g_timer_gate);
}

int IRAM_ATTR wr_btdm_osal_callout_reset(struct btdm_osal_callout *co,
                                       uint32_t ms)
{
  struct s31_callout_s *c = co->co;
  irqstate_t flags;
  int ret;
  if (!c || ms > INT32_MAX)
    {
      return BTDM_OSAL_INVALID_PARM;
    }

  flags = enter_critical_section();
  wr_btdm_osal_callout_stop(co);
  c->expiry = wr_btdm_osal_time_get() + MAX(1, ms);
  ret = wd_start(&c->wd, MAX(1, MSEC2TICK((uint64_t)ms)),
                 s31_expire, (wdparm_t)(uintptr_t)c);
  leave_critical_section(flags);
  return s31_result(ret);
}

void wr_btdm_osal_callout_mem_reset(struct btdm_osal_callout *co)
{
  if (co->co)
    {
      wr_btdm_osal_event_reset(&((struct s31_callout_s *)co->co)->event);
    }
}

bool IRAM_ATTR wr_btdm_osal_callout_is_active(struct btdm_osal_callout *co)
{
  struct s31_callout_s *c = co->co;
  return c && WDOG_ISACTIVE(&c->wd);
}

uint32_t IRAM_ATTR wr_btdm_osal_callout_get_ticks(struct btdm_osal_callout *co)
{
  struct s31_callout_s *c = co->co;
  return c && WDOG_ISACTIVE(&c->wd) ? c->expiry : 0;
}

uint32_t IRAM_ATTR wr_btdm_osal_callout_remaining_ticks(
    struct btdm_osal_callout *co, uint32_t now)
{
  struct s31_callout_s *c = co->co;
  int32_t remaining;
  if (!c || !WDOG_ISACTIVE(&c->wd))
    {
      return 0;
    }

  remaining = (int32_t)(c->expiry - now);
  return remaining > 0 ? remaining : 0;
}

void IRAM_ATTR wr_btdm_osal_callout_set_arg(struct btdm_osal_callout *co,
                                          void *arg)
{
  wr_btdm_osal_event_set_arg(&((struct s31_callout_s *)co->co)->event, arg);
}

uint32_t IRAM_ATTR wr_btdm_osal_time_get(void)
{
  return (uint32_t)TICK2MSEC(clock_systime_ticks());
}

int IRAM_ATTR wr_btdm_osal_time_ms_to_ticks(uint32_t ms, uint32_t *out)
{
  if (!out)
    {
      return BTDM_OSAL_INVALID_PARM;
    }

  *out = ms;
  return BTDM_OSAL_OK;
}

int IRAM_ATTR wr_btdm_osal_time_ticks_to_ms(uint32_t ticks, uint32_t *out)
{
  return wr_btdm_osal_time_ms_to_ticks(ticks, out);
}

uint32_t IRAM_ATTR wr_btdm_osal_time_ms_to_ticks32(uint32_t ms)
{
  return ms;
}

uint32_t IRAM_ATTR wr_btdm_osal_time_ticks_to_ms32(uint32_t ticks)
{
  return ticks;
}

uint32_t IRAM_ATTR wr_btdm_osal_get_time_forever(void)
{
  return UINT32_MAX;
}

uint32_t IRAM_ATTR wr_btdm_osal_hw_enter_critical(void)
{
  irqstate_t flags = enter_critical_section();
  ASSERT(g_critical_nest < sizeof(g_critical_state) / sizeof(g_critical_state[0]));
  g_critical_state[g_critical_nest++] = flags;
  return 0;
}

void IRAM_ATTR wr_btdm_osal_hw_exit_critical(uint32_t ctx)
{
  /* Pinned HCI code discards enter's return and calls exit(0). */

  (void)ctx;
  ASSERT(g_critical_nest > 0);
  leave_critical_section(g_critical_state[--g_critical_nest]);
}

uint8_t IRAM_ATTR wr_btdm_osal_hw_is_in_critical(void)
{
  return MIN(g_critical_nest, UINT8_MAX);
}

static int s31_task_main(int argc, char **argv)
{
  btdm_osal_task_fn *fn =
    (btdm_osal_task_fn *)(uintptr_t)strtoul(argv[1], NULL, 16);
  void *arg = (void *)(uintptr_t)strtoul(argv[2], NULL, 16);
#ifdef CONFIG_SMP
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  if (nxsched_set_affinity(0, sizeof(cpuset), &cpuset) < 0)
    {
      return ERROR;
    }
#endif
  fn(arg);
  return 0;
}

int wr_btdm_osal_task_create(btdm_osal_task_fn *fn, const char *name,
                           uint32_t stack, void *arg, uint32_t priority,
                           void **handle, uint32_t core)
{
  char address[2 * sizeof(uintptr_t) + 1];
  char argument[2 * sizeof(uintptr_t) + 1];
  char *argv[] = { address, argument, NULL };
  int pid;
  if (!fn || !handle || stack > INT_MAX || priority > 25)
    {
      return BTDM_OSAL_INVALID_PARM;
    }

  /* FreeRTOS priorities increase with urgency. Reserve 100..125, above
   * ordinary shell tasks and below the NuttX high-priority worker.
   * Controller task entry pins kernel/SMP execution to the radio CPU0.
   */

  (void)core;
  *handle = NULL;
  snprintf(address, sizeof(address), "%lx", (unsigned long)(uintptr_t)fn);
  snprintf(argument, sizeof(argument), "%lx", (unsigned long)(uintptr_t)arg);
  /* FLAT HCI operations run in application threads. Kernel HCI operations
   * are dispatched by ble_ctrl, including task creation and deletion.
   * The FLAT HCI device is opened and closed by application threads.
   * Controller tasks must therefore be ordinary tasks: nxtask_delete()
   * correctly rejects application attempts to delete kernel threads.
   */

#ifdef CONFIG_BUILD_KERNEL
  pid = kthread_create(name, 100 + priority, stack, s31_task_main, argv);
#else
  pid = task_create(name, 100 + priority, stack, s31_task_main, argv);
#endif
  if (pid < 0)
    {
      return BTDM_OSAL_ERROR;
    }

  *handle = (void *)(uintptr_t)pid;
  return BTDM_OSAL_OK;
}

void wr_btdm_osal_task_delete(void *handle)
{
  int ret = nxtask_delete((pid_t)(uintptr_t)handle);
  DEBUGASSERT(ret >= 0);
  (void)ret;
}

static int IRAM_ATTR s31_irq(int irq, void *context, void *arg)
{
  struct s31_intr_s *intr = arg;
  intr->fn(intr->arg);
  return 0;
}

int wr_btdm_osal_intr_alloc(int source, int flags, btdm_osal_intr_fn *fn,
                          void *arg, void **handle)
{
  struct s31_intr_s *intr;
  int ret;
  int irq;
  if (!fn || !handle || source < 0 || ESP_SOURCE2IRQ(source) >= NR_IRQS)
    {
      return -EINVAL;
    }

  intr = kmm_zalloc(sizeof(*intr));
  if (!intr)
    {
      return -ENOMEM;
    }

  intr->fn = fn;
  intr->arg = arg;
  intr->source = source;
  intr->cpuint = esp_setup_irq(source, 3, ESP_IRQ_TRIGGER_LEVEL |
                               (flags == 0 ? ESP_IRQ_IRAM : 0));
  if (intr->cpuint < 0)
    {
      ret = intr->cpuint;
      goto fail;
    }

  irq = ESP_SOURCE2IRQ(source);
  ret = irq_attach(irq, s31_irq, intr);
  if (ret < 0)
    {
      esp_teardown_irq(source, intr->cpuint);
      goto fail;
    }

  *handle = intr;
  up_enable_irq(irq);
  return 0;
fail:
  kmm_free(intr);
  return ret;
}

int wr_btdm_osal_intr_free(void *handle)
{
  struct s31_intr_s *intr = handle;
  if (!intr || up_interrupt_context())
    {
      return -EINVAL;
    }

  up_disable_irq(ESP_SOURCE2IRQ(intr->source));
  irq_detach(ESP_SOURCE2IRQ(intr->source));
  esp_teardown_irq(intr->source, intr->cpuint);
  kmm_free(intr);
  return 0;
}

void *wr_btdm_osal_malloc(uint32_t size, btdm_osal_malloc_flag_t flags)
{
  (void)flags;
  return kmm_malloc(size);
}

void wr_btdm_osal_free(void *ptr)
{
  kmm_free(ptr);
}

#if !CONFIG_BT_CTRL_MULTI_LINK_ENABLED
void *wr_btdm_osal_mmgmt_block_malloc(uint32_t size)
{
  uint32_t *base;
  if (size > UINT32_MAX - 4)
    {
      return NULL;
    }

  base = kmm_malloc(size + 4);
  if (!base)
    {
      return NULL;
    }

  *base = (5u << 29) | ((size + 4) >> 2);
  return base + 1;
}

void wr_btdm_osal_mmgmt_block_free(void *ptr)
{
  if (ptr)
    {
      kmm_free((uint32_t *)ptr - 1);
    }
}

void wr_btdm_osal_mmgmt_block_copy(void *dst, const void *src, uint16_t size)
{
  extern void r_ble_lll_mmgmt_block_copy(void *, void *, uint16_t);
  r_ble_lll_mmgmt_block_copy(dst, (void *)src, size);
}
#endif

int wr_btdm_osal_read_efuse_mac(uint8_t *mac)
{
  return esp_read_mac(mac, ESP_MAC_BT);
}

void wr_btdm_osal_srand(uint32_t seed)
{
  srand(seed);
}

int IRAM_ATTR wr_btdm_osal_rand(void)
{
  return (int)esp_random();
}

void IRAM_ATTR wr_btdm_osal_ets_delay_us(uint32_t us)
{
  esp_rom_delay_us(us);
}

static int s31_timer_main(int argc, char **argv)
{
  struct s31_queue_s *q = g_timerq.eventq;
  for (;;)
    {
      struct btdm_osal_event *ev;
      nxsem_wait_uninterruptible(&q->ready);
      nxrmutex_lock(&g_timer_gate);
      while ((ev = wr_btdm_osal_eventq_get(&g_timerq, 0)) != NULL)
        {
          wr_btdm_osal_event_run(ev);
        }

      nxrmutex_unlock(&g_timer_gate);
    }

  return 0;
}

int btdm_osal_elem_mempool_init(btdm_osal_elem_num_t *num)
{
  unsigned int counts[NPOOLS];
  const size_t sizes[NPOOLS] =
    {
      sizeof(struct s31_event_s), sizeof(struct s31_queue_s),
      sizeof(struct s31_callout_s), sizeof(sem_t), sizeof(rmutex_t)
    };
  unsigned int i;
  if (!num || g_pools[EVENTS].storage || g_timer_pid > 0)
    {
      return -EINVAL;
    }

  counts[EVENTS] = num->evt_count;
  counts[QUEUES] = (unsigned int)num->evtq_count + 1;
  counts[CALLOUTS] = num->co_count;
  counts[SEMAPHORES] = num->sem_count;
  counts[MUTEXES] = num->mutex_count;
  for (i = 0; i < NPOOLS; i++)
    {
      struct s31_pool_s *p = &g_pools[i];
      p->count = counts[i];
      p->size = sizes[i];
      if (p->count)
        {
          p->storage = kmm_zalloc(p->size * p->count);
          p->used = kmm_zalloc(p->count);
          if (!p->storage || !p->used)
            {
              goto fail;
            }
        }
    }

  wr_btdm_osal_eventq_init(&g_timerq);
  g_timer_pid = kthread_create("btdm_timer", 124, 3072,
                               s31_timer_main, NULL);
  if (g_timer_pid < 0)
    {
      wr_btdm_osal_eventq_deinit(&g_timerq);
      goto fail;
    }

  return 0;
fail:
  for (i = 0; i < NPOOLS; i++)
    {
      kmm_free(g_pools[i].used);
      kmm_free(g_pools[i].storage);
      memset(&g_pools[i], 0, sizeof(g_pools[i]));
    }

  return -ENOMEM;
}

void btdm_osal_elem_mempool_deinit(void)
{
  unsigned int i;
  unsigned int j;
  /* Do not mask leaked controller objects by freeing live pools. */

  if (g_timer_pid > 0)
    {
      nxrmutex_lock(&g_timer_gate);
      nxtask_delete(g_timer_pid);
      g_timer_pid = -1;
      wr_btdm_osal_eventq_deinit(&g_timerq);
      nxrmutex_unlock(&g_timer_gate);
    }

  for (i = 0; i < NPOOLS; i++)
    {
      for (j = 0; j < g_pools[i].count; j++)
        {
          ASSERT(g_pools[i].used[j] == 0);
        }

      kmm_free(g_pools[i].used);
      kmm_free(g_pools[i].storage);
      memset(&g_pools[i], 0, sizeof(g_pools[i]));
    }
}
