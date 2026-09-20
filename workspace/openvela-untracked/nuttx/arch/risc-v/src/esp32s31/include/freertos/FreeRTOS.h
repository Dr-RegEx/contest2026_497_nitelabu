/****************************************************************************
 * arch/risc-v/src/esp32s31/include/freertos/FreeRTOS.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal critical-section compatibility used by the ESP-IDF camera HAL.
 * Also supplies the small task/queue/semaphore API subset used by SDMMC.
 * This is not a general FreeRTOS implementation.
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_INCLUDE_FREERTOS_FREERTOS_H
#define __ARCH_RISCV_SRC_ESP32S31_INCLUDE_FREERTOS_FREERTOS_H

#include <nuttx/config.h>
#include <nuttx/percpu.h>
#include <nuttx/clock.h>
#include <stdint.h>
#include <nuttx/spinlock.h>

#ifndef CONFIG_NCPUS
#  define CONFIG_NCPUS 1
#endif

/* SD host synchronization uses NuttX scheduler ticks. */
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define pdFAIL pdFALSE
#define portMAX_DELAY UINT32_MAX
#if defined(CONFIG_ESP32S31_SDMMC) && \
    (USEC_PER_TICK < 1000 || (USEC_PER_TICK % 1000) != 0)
#  error "SDMMC IDF host requires a whole-millisecond NuttX tick"
#endif
#define portTICK_PERIOD_MS (USEC_PER_TICK / 1000)
#define pdMS_TO_TICKS(ms) ((TickType_t)MSEC2TICK(ms))

/* NuttX semaphore posts in IRQ context already arrange rescheduling on IRQ
 * return; there is no second FreeRTOS scheduler to invoke here. */
#define portYIELD_FROM_ISR() do { } while (0)

/* IDF's portMUX is a recursive, interrupt-masking spinlock.  NuttX's
 * rspinlock provides the same ownership and nesting semantics.  The IDF
 * macros do not carry an irqstate argument, so retain the state from the
 * outermost acquisition per CPU and restore it when the final nested level
 * is released.
 */
typedef struct portMUX_TYPE
{
  rspinlock_t lock;
  irqstate_t irqstate[CONFIG_NCPUS];
} portMUX_TYPE;

#define portMUX_INITIALIZER_UNLOCKED \
  { RSPINLOCK_INITIALIZER, { 0 } }

static inline_function void
vela_port_enter_critical(FAR portMUX_TYPE *mux)
{
  irqstate_t flags = rspin_lock_irqsave(&mux->lock);

  if (mux->lock.count == 1)
    {
      mux->irqstate[this_cpu()] = flags;
    }
}

static inline_function void
vela_port_exit_critical(FAR portMUX_TYPE *mux)
{
  rspin_unlock_irqrestore(&mux->lock, mux->irqstate[this_cpu()]);
}

#define portENTER_CRITICAL(mux) vela_port_enter_critical((mux))
#define portEXIT_CRITICAL(mux)  vela_port_exit_critical((mux))

#endif /* __ARCH_RISCV_SRC_ESP32S31_INCLUDE_FREERTOS_FREERTOS_H */
