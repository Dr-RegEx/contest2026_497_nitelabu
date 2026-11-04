/****************************************************************************
 * arch/risc-v/src/esp32s31/include/irq.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_INCLUDE_IRQ_H
#define __ARCH_RISCV_SRC_ESP32S31_INCLUDE_IRQ_H

#include <nuttx/config.h>

/* ESP32-S31 exposes 32 CLIC CPU interrupt inputs.  Keep the allocation mask
 * aligned with the existing dual-core CLIC ESP32-P4 port until the reserved
 * interrupt assignments are published for S31.
 */

/* Keep this numeric for assembly users of irq.h.  The S31 interrupt source
 * enum currently contains IDs 0..168, with ETS_MAX_INTR_SOURCE == 169.
 */

#define ESP_NSOURCES            169
#define ESP_NCPUINTS            32

#ifdef CONFIG_ESP32S31_RWDT
/* S31 has three CLIC level bits. Mask levels 1..6, retaining level 7 for
 * the fatal watchdog interrupt. Capture callbacks run at normal level 1.
 */

#  define RISCV_MAX_INTTHRESH   0xdf
#endif

/* CPU interrupt 1 is the fixed Wi-Fi MAC input selected by the vendor
 * binary.  Exclude it from the generic allocator so an earlier peripheral
 * (notably SYSTIMER target 0) cannot occupy the same CLIC input.
 */

#define ESP_CPUINT_PERIPHSET    0xdffe773d

/* NuttX reserves exception/ECALL slots 0..15 and one demultiplexer IRQ. */

#define RISCV_NIRQ_INTERRUPTS   16
#define ESP_NIRQ_DEMUX          1
#define ESP_IRQ_DEMUX           RISCV_NIRQ_INTERRUPTS
#define ESP_IRQ_FIRSTPERIPH     (RISCV_NIRQ_INTERRUPTS + ESP_NIRQ_DEMUX)

#define ESP_IRQ2SOURCE(irq)     ((irq) - ESP_IRQ_FIRSTPERIPH)
#define ESP_SOURCE2IRQ(source)  ((source) + ESP_IRQ_FIRSTPERIPH)

#define ESP_NIRQ_PERIPH         ESP_NSOURCES

#ifdef CONFIG_ESPRESSIF_GPIO_IRQ
#  define ESP_NIRQ_GPIO         62
#  define ESP_FIRST_GPIOIRQ     (ESP_IRQ_FIRSTPERIPH + ESP_NIRQ_PERIPH)
#  define ESP_LAST_GPIOIRQ      (ESP_FIRST_GPIOIRQ + ESP_NIRQ_GPIO - 1)
#  define ESP_PIN2IRQ(pin)      ((pin) + ESP_FIRST_GPIOIRQ)
#  define ESP_IRQ2PIN(irq)      ((irq) - ESP_FIRST_GPIOIRQ)
#else
#  define ESP_NIRQ_GPIO         0
#endif

#define NR_IRQS (RISCV_NIRQ_INTERRUPTS + ESP_NIRQ_DEMUX + \
                 ESP_NIRQ_PERIPH + ESP_NIRQ_GPIO)

#if defined(CONFIG_ARCH_MINIMAL_VECTORTABLE) && \
    CONFIG_ARCH_NUSER_INTERRUPTS < (ESP_IRQ_DEMUX + 1)
#  error "CONFIG_ARCH_NUSER_INTERRUPTS must include the ESP IRQ demultiplexer"
#endif

#endif /* __ARCH_RISCV_SRC_ESP32S31_INCLUDE_IRQ_H */
