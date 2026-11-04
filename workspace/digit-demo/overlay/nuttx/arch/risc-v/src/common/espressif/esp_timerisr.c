/****************************************************************************
 * arch/risc-v/src/common/espressif/esp_timerisr.c
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

#include <assert.h>
#include <stdint.h>
#include <time.h>

#include <arch/board/board.h>
#include <arch/irq.h>

#include "chip.h"
#include "esp_irq.h"

#include "hal/systimer_hal.h"
#include "hal/systimer_ll.h"
#include "periph_ctrl.h"
#include "systimer.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if SYSTIMER_LL_INT_LEVEL
#  define SYSTIMER_TRIGGER_TYPE ESP_IRQ_TRIGGER_LEVEL
#else
#  define SYSTIMER_TRIGGER_TYPE ESP_IRQ_TRIGGER_EDGE
#endif /* SYSTIMER_LL_INT_LEVEL */

#ifdef CONFIG_ARCH_CHIP_ESP32S31
#  define CHIP_SYSTIMER_SOURCE   ETS_SYSTIMER_TARGET0_INTR_SOURCE
#  define CHIP_SYSTIMER_IRQ      ESP_SOURCE2IRQ(CHIP_SYSTIMER_SOURCE)
#  define CHIP_SYSTIMER_PRIORITY ESP_IRQ_PRIORITY_2
#else
#  define CHIP_SYSTIMER_SOURCE   SYSTIMER_TARGET0_EDGE_INTR_SOURCE
#  define CHIP_SYSTIMER_IRQ      ESP_IRQ_SYSTIMER_TARGET0_EDGE
#  define CHIP_SYSTIMER_PRIORITY ESP_IRQ_PRIORITY_DEFAULT
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Systimer HAL layer object */

static systimer_hal_context_t systimer_hal;

#ifdef CONFIG_ARCH_CHIP_ESP32S31
static uint64_t g_s31_handled_ticks;
static bool g_s31_tick_started;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: systimer_irq_handler
 *
 * Description:
 *   Handler to be executed by the Systimer ISR.
 *
 * Input Parameters:
 *   irq           - IRQ associated to that interrupt.
 *   context       - Interrupt register state save info.
 *   arg           - A pointer to the argument provided when the interrupt
 *                   was registered.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int systimer_irq_handler(int irq, void *context, void *arg)
{
  systimer_ll_clear_alarm_int(systimer_hal.dev,
                              SYSTIMER_ALARM_OS_TICK_CORE0);

#ifdef CONFIG_ARCH_CHIP_ESP32S31
  uint64_t ticks = systimer_hal_get_time(&systimer_hal,
                                        SYSTIMER_COUNTER_OS_TICK) /
                   CONFIG_USEC_PER_TICK;
  uint64_t pending;

  /* Periodic interrupt status coalesces when Wi-Fi masks interrupts for
   * multiple periods. Account elapsed hardware time, not just ISR entries.
   * CPU0 remains the sole owner of the global scheduler tick.
   */

  if (!g_s31_tick_started)
    {
      g_s31_tick_started = true;
      g_s31_handled_ticks = ticks;
      pending = 1;
    }
  else
    {
      pending = ticks - g_s31_handled_ticks;
      if (pending > 32)
        {
          pending = 32;
        }

      /* Keep any remaining debt for subsequent periodic interrupts. */

      g_s31_handled_ticks += pending;
    }

  while (pending-- > 0)
    {
      nxsched_process_timer();
    }
#else
  /* Process timer interrupt */

  nxsched_process_timer();
#endif

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_timer_initialize
 *
 * Description:
 *   This function is called during start-up to initialize the timer
 *   interrupt.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

void up_timer_initialize(void)
{
#ifdef CONFIG_ARCH_CHIP_ESP32S31
  PERIPH_RCC_ACQUIRE_ATOMIC(PERIPH_SYSTIMER_MODULE, ref_count)
    {
      if (ref_count == 0)
        {
          systimer_ll_enable_bus_clock(true);
          systimer_ll_reset_register();
          systimer_ll_enable_sys_clock(true);
        }
    }
#else
  periph_module_enable(PERIPH_SYSTIMER_MODULE);
#endif
  systimer_hal_init(&systimer_hal);
  systimer_hal_tick_rate_ops_t ops =
    {
      .ticks_to_us = systimer_ticks_to_us,
      .us_to_ticks = systimer_us_to_ticks,
    };

  systimer_hal_set_tick_rate_ops(&systimer_hal, &ops);
  systimer_ll_set_counter_value(systimer_hal.dev,
                                SYSTIMER_COUNTER_OS_TICK,
                                0);
  systimer_ll_apply_counter_value(systimer_hal.dev,
                                  SYSTIMER_COUNTER_OS_TICK);

#ifdef CONFIG_ARCH_CHIP_ESP32S31
  {
    int cpu;

    /* UNIT1 stall-enable resets asserted for both cores.  Clear every
     * core before enabling stall only for the CPU running NuttX.
     */

    for (cpu = 0; cpu < SOC_CPU_CORES_NUM; cpu++)
      {
        systimer_hal_counter_can_stall_by_cpu(&systimer_hal,
                                              SYSTIMER_COUNTER_OS_TICK,
                                              cpu, false);
      }
  }
#else
  systimer_hal_counter_can_stall_by_cpu(&systimer_hal,
                                        SYSTIMER_COUNTER_OS_TICK, 0,
                                        false);
#endif

  systimer_hal_connect_alarm_counter(&systimer_hal,
                                     SYSTIMER_ALARM_OS_TICK_CORE0,
                                     SYSTIMER_COUNTER_OS_TICK);
  systimer_hal_set_alarm_period(&systimer_hal,
                                SYSTIMER_ALARM_OS_TICK_CORE0,
                                CONFIG_USEC_PER_TICK);
  systimer_hal_select_alarm_mode(&systimer_hal,
                                 SYSTIMER_ALARM_OS_TICK_CORE0,
                                 SYSTIMER_ALARM_MODE_PERIOD);
  systimer_hal_counter_can_stall_by_cpu(&systimer_hal,
                                        SYSTIMER_COUNTER_OS_TICK, 0,
                                        true);
  systimer_hal_enable_alarm_int(&systimer_hal,
                                SYSTIMER_ALARM_OS_TICK_CORE0);
  systimer_hal_enable_counter(&systimer_hal, SYSTIMER_COUNTER_OS_TICK);

  esp_setup_irq(CHIP_SYSTIMER_SOURCE,
                CHIP_SYSTIMER_PRIORITY,
                SYSTIMER_TRIGGER_TYPE);

  /* Attach the timer interrupt. */

  irq_attach(CHIP_SYSTIMER_IRQ,
             (xcpt_t)systimer_irq_handler,
             NULL);

  /* Enable the allocated CPU interrupt. */

  up_enable_irq(CHIP_SYSTIMER_IRQ);
}
