/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_sdmmc_os.c
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bounded SD host synchronization.  Delete only after controller IRQs are
 * disabled and all users have quiesced, as required by the IDF host driver.
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/signal.h>
#include <nuttx/spinlock.h>
#include <nuttx/clock.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sched.h>
#include "esp_heap_caps.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"

struct esp32s31_sd_sem_s
{
  bool mutex;
  spinlock_t lock;
  union
  {
    sem_t event;
    mutex_t mutex;
  } u;
};

struct esp32s31_sd_queue_s
{
  sem_t ready;
  spinlock_t lock;
  size_t length;
  size_t size;
  size_t head;
  size_t tail;
  size_t count;
  unsigned char data[];
};

static int sd_wait(sem_t *sem, TickType_t ticks)
{
  if (ticks == 0)
    {
      return nxsem_trywait(sem);
    }

  if (ticks == portMAX_DELAY)
    {
      return nxsem_wait_uninterruptible(sem);
    }

  return nxsem_tickwait_uninterruptible(sem, ticks);
}

static SemaphoreHandle_t sd_sem_create(bool mutex, UBaseType_t caps)
{
  SemaphoreHandle_t sem = heap_caps_calloc(1, sizeof(*sem), caps);
  int ret;

  if (sem == NULL)
    {
      return NULL;
    }

  sem->mutex = mutex;
  spin_lock_init(&sem->lock);
  if (mutex)
    {
      ret = nxmutex_init(&sem->u.mutex);
    }
  else
    {
      ret = nxsem_init(&sem->u.event, 0, 0);
      if (ret == 0)
        {
          nxsem_set_protocol(&sem->u.event, SEM_PRIO_NONE);
        }
    }

  if (ret < 0)
    {
      heap_caps_free(sem);
      return NULL;
    }

  return sem;
}

SemaphoreHandle_t xSemaphoreCreateBinaryWithCaps(UBaseType_t caps)
{
  return sd_sem_create(false, caps);
}

SemaphoreHandle_t xSemaphoreCreateMutexWithCaps(UBaseType_t caps)
{
  return sd_sem_create(true, caps);
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks)
{
  int ret;

  if (sem == NULL || up_interrupt_context())
    {
      return pdFALSE;
    }

  if (!sem->mutex)
    {
      ret = sd_wait(&sem->u.event, ticks);
    }
  else if (ticks == portMAX_DELAY)
    {
      ret = nxmutex_lock(&sem->u.mutex);
    }
  else
    {
      ret = nxmutex_ticklock(&sem->u.mutex, ticks);
    }

  return ret == 0 ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t sem)
{
  irqstate_t flags;
  int count;
  int ret;

  if (sem == NULL)
    {
      return pdFALSE;
    }

  if (sem->mutex)
    {
      if (up_interrupt_context())
        {
          return pdFALSE;
        }

      return nxmutex_unlock(&sem->u.mutex) == 0 ? pdTRUE : pdFALSE;
    }

  /* Posting may immediately run a higher-priority waiter. Keep this task
   * non-preemptible until the private lock has been released; otherwise a
   * resumed waiter giving the same binary semaphore can spin on its
   * suspended signaller. Interrupt callers are already non-preemptible.
   */

#ifdef CONFIG_SPINLOCK
  flags = spin_lock_irqsave_nopreempt(&sem->lock);
#else
  flags = spin_lock_irqsave(&sem->lock);
#endif
  ret = nxsem_get_value(&sem->u.event, &count);
  if (ret == 0)
    {
      ret = count < 1 ? nxsem_post(&sem->u.event) : -EAGAIN;
    }

#ifdef CONFIG_SPINLOCK
  spin_unlock_irqrestore_nopreempt(&sem->lock, flags);
#else
  spin_unlock_irqrestore(&sem->lock, flags);
#endif
  return ret == 0 ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *woken)
{
  BaseType_t ret;

  if (sem == NULL || sem->mutex)
    {
      return pdFALSE;
    }

  ret = xSemaphoreGive(sem);
  if (ret == pdTRUE && woken != NULL)
    {
      *woken = pdTRUE;
    }

  return ret;
}

void vSemaphoreDeleteWithCaps(SemaphoreHandle_t sem)
{
  if (sem == NULL)
    {
      return;
    }

  if (sem->mutex)
    {
      nxmutex_destroy(&sem->u.mutex);
    }
  else
    {
      nxsem_destroy(&sem->u.event);
    }

  heap_caps_free(sem);
}

QueueHandle_t xQueueCreateWithCaps(UBaseType_t count, UBaseType_t size,
                                 UBaseType_t caps)
{
  QueueHandle_t q;

  if (count == 0 || size == 0 ||
      count > (SIZE_MAX - sizeof(*q)) / size)
    {
      return NULL;
    }

  q = heap_caps_calloc(1, sizeof(*q) + (size_t)count * size, caps);
  if (q == NULL)
    {
      return NULL;
    }

  q->length = count;
  q->size = size;
  spin_lock_init(&q->lock);
  if (nxsem_init(&q->ready, 0, 0) < 0)
    {
      heap_caps_free(q);
      return NULL;
    }

  nxsem_set_protocol(&q->ready, SEM_PRIO_NONE);
  return q;
}

BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *item,
                           BaseType_t *woken)
{
  irqstate_t flags;
  int ret;

  if (q == NULL || item == NULL)
    {
      return pdFALSE;
    }

  flags = spin_lock_irqsave(&q->lock);
  if (q->count == q->length)
    {
      spin_unlock_irqrestore(&q->lock, flags);
      return pdFALSE;
    }

  memcpy(q->data + q->tail * q->size, item, q->size);
  q->tail = (q->tail + 1) % q->length;
  q->count++;
  ret = nxsem_post(&q->ready);
  if (ret < 0)
    {
      q->tail = (q->tail + q->length - 1) % q->length;
      q->count--;
    }

  spin_unlock_irqrestore(&q->lock, flags);
  if (ret == 0 && woken != NULL)
    {
      *woken = pdTRUE;
    }

  return ret == 0 ? pdTRUE : pdFALSE;
}

BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t ticks)
{
  irqstate_t flags;

  if (q == NULL || item == NULL || up_interrupt_context() ||
      sd_wait(&q->ready, ticks) < 0)
    {
      return pdFALSE;
    }

  flags = spin_lock_irqsave(&q->lock);
  memcpy(item, q->data + q->head * q->size, q->size);
  q->head = (q->head + 1) % q->length;
  q->count--;
  spin_unlock_irqrestore(&q->lock, flags);
  return pdTRUE;
}

void vQueueDeleteWithCaps(QueueHandle_t q)
{
  if (q != NULL)
    {
      nxsem_destroy(&q->ready);
      heap_caps_free(q);
    }
}

void vTaskDelay(TickType_t ticks)
{
  struct timespec remain;

  if (ticks == 0)
    {
      sched_yield();
      return;
    }

  clock_ticks2time(&remain, ticks);
  while (nxsig_nanosleep(&remain, &remain) == -EINTR)
    {
    }
}

#if !defined(CONFIG_ESPRESSIF_WIFI) && !defined(CONFIG_ESP32S31_BLUETOOTH)
#  if defined(CONFIG_RTC_DRIVER) && defined(CONFIG_ESPRESSIF_HR_TIMER)
#    include "esp_hr_timer.h"
#  elif defined(CONFIG_SMP)
#    error "Standalone SMP SDMMC requires the RTC-initialized HR timer"
#  endif

/* The board initializes the NuttX HR timer through the RTC driver before
 * probing SD.  Use that shared clock when available: per-CPU counters are
 * unsuitable for deadlines across task migration.  Do not initialize a
 * second SYSTIMER owner here.  Legacy single-core profiles without RTC keep
 * the counter fallback at their fixed CPU frequency.
 */
uint64_t __wrap_esp_timer_get_time(void)
{
#if defined(CONFIG_RTC_DRIVER) && defined(CONFIG_ESPRESSIF_HR_TIMER)
  return esp_hr_timer_time_us();
#else
  uint32_t hi, lo, check;
  do
    {
#ifdef CONFIG_ARCH_USE_S_MODE
      /* The monitor permits the read-only cycle aliases in S-mode, not
       * the machine-only mcycle CSR addresses.
       */

      __asm__ volatile("csrr %0, cycleh" : "=r"(hi));
      __asm__ volatile("csrr %0, cycle" : "=r"(lo));
      __asm__ volatile("csrr %0, cycleh" : "=r"(check));
#else
      __asm__ volatile("csrr %0, mcycleh" : "=r"(hi));
      __asm__ volatile("csrr %0, mcycle" : "=r"(lo));
      __asm__ volatile("csrr %0, mcycleh" : "=r"(check));
#endif
    }
  while (hi != check);
  return (((uint64_t)hi << 32) | lo) / CONFIG_ESPRESSIF_CPU_FREQ_MHZ;
#endif
}
#endif
