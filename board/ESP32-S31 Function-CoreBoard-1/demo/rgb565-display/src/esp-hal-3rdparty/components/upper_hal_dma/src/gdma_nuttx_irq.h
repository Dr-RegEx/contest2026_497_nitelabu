/* SPDX-License-Identifier: Apache-2.0
 * ESP32-S31 GDMA adapter for the NuttX interrupt allocator.
 * Vendor intr_alloc owns a separate CPU-vector table; routing its vectors
 * directly bypasses NuttX's CPU-input map. Keep allocation and dispatch in
 * NuttX, including RX/TX handlers sharing one peripheral interrupt source.
 * Private to gdma.c: these handles must not escape to vendor intr_alloc.
 */
#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include "esp_irq.h"

struct s31_gdma_irq {
    struct s31_gdma_irq *next;
    intr_handler_t handler;
    void *arg;
    uint32_t statusreg;
    uint32_t statusmask;
    int source;
    int cpuint;
    int priority;
    bool iram;
    bool enabled;
};

static struct s31_gdma_irq *s31_gdma_irqs;

static int IRAM_ATTR s31_gdma_dispatch(int irq, void *context, void *arg)
{
    int source = (int)(intptr_t)arg;
    (void)irq;
    (void)context;
    struct s31_gdma_irq *entry;
    for (entry = s31_gdma_irqs; entry; entry = entry->next) {
        if (entry->source == source && entry->enabled &&
            (!entry->statusreg ||
             (*(volatile uint32_t *)(uintptr_t)entry->statusreg & entry->statusmask))) {
            entry->handler(entry->arg);
        }
    }
    return 0;
}

static esp_err_t s31_gdma_intr_alloc(const esp_intr_alloc_info_t *info,
                                      intr_handle_t *handle)
{
    struct s31_gdma_irq *entry;
    struct s31_gdma_irq *shared;
    irqstate_t flags;
    int priority = 1;
    int irq;
    int ret;
    if (!info || !handle || !info->handler || info->source < 0 ||
        info->source >= ESP_NSOURCES || (info->flags & ESP_INTR_FLAG_EDGE)) {
        return ESP_ERR_INVALID_ARG;
    }
    while (priority < 7 && !(info->flags & (1 << priority))) priority++;
    if (priority == 7 && !(info->flags & (1 << priority))) priority = 1;
    entry = kmm_zalloc(sizeof(*entry));
    if (!entry) return ESP_ERR_NO_MEM;
    entry->handler = info->handler;
    entry->arg = info->arg;
    entry->statusreg = info->intrstatusreg;
    entry->statusmask = info->intrstatusmask;
    entry->source = info->source;
    entry->priority = priority;
    entry->iram = (info->flags & ESP_INTR_FLAG_IRAM) != 0;
    entry->enabled = !(info->flags & ESP_INTR_FLAG_INTRDISABLED);
    irq = ESP_SOURCE2IRQ(entry->source);
    flags = enter_critical_section();
    for (shared = s31_gdma_irqs; shared; shared = shared->next) {
        if (shared->source == entry->source) break;
    }
    if (shared) {
        if (shared->priority != priority || shared->iram != entry->iram) {
            leave_critical_section(flags);
            kmm_free(entry);
            return ESP_ERR_INVALID_STATE;
        }
        entry->cpuint = shared->cpuint;
    } else {
        entry->cpuint = esp_setup_irq(entry->source, priority,
                                     entry->iram ? ESP_IRQ_IRAM : ESP_IRQ_NON_IRAM);
        if (entry->cpuint < 0) {
            leave_critical_section(flags);
            kmm_free(entry);
            return ESP_ERR_NO_MEM;
        }
        up_disable_irq(irq);
        ret = irq_attach(irq, s31_gdma_dispatch, (void *)(intptr_t)entry->source);
        if (ret < 0) {
            esp_teardown_irq(entry->source, entry->cpuint);
            leave_critical_section(flags);
            kmm_free(entry);
            return ESP_FAIL;
        }
    }
    entry->next = s31_gdma_irqs;
    s31_gdma_irqs = entry;
    if (entry->enabled) up_enable_irq(irq);
    *handle = (intr_handle_t)entry;
    leave_critical_section(flags);
    return ESP_OK;
}

static esp_err_t s31_gdma_intr_enable(intr_handle_t handle)
{
    struct s31_gdma_irq *entry = (struct s31_gdma_irq *)handle;
    irqstate_t flags;
    if (!entry) return ESP_ERR_INVALID_ARG;
    flags = enter_critical_section();
    entry->enabled = true;
    up_enable_irq(ESP_SOURCE2IRQ(entry->source));
    leave_critical_section(flags);
    return ESP_OK;
}

static esp_err_t s31_gdma_intr_free(intr_handle_t handle)
{
    struct s31_gdma_irq *entry = (struct s31_gdma_irq *)handle;
    struct s31_gdma_irq **link;
    struct s31_gdma_irq *other;
    bool shared = false;
    bool enabled = false;
    irqstate_t flags;
    if (!entry) return ESP_ERR_INVALID_ARG;
    flags = enter_critical_section();
    for (link = &s31_gdma_irqs; *link && *link != entry; link = &(*link)->next);
    if (!*link) {
        leave_critical_section(flags);
        return ESP_ERR_INVALID_ARG;
    }
    *link = entry->next;
    for (other = s31_gdma_irqs; other; other = other->next) {
        if (other->source == entry->source) {
            shared = true;
            enabled |= other->enabled;
        }
    }
    if (!enabled) up_disable_irq(ESP_SOURCE2IRQ(entry->source));
    if (!shared) {
        irq_detach(ESP_SOURCE2IRQ(entry->source));
        esp_teardown_irq(entry->source, entry->cpuint);
    }
    leave_critical_section(flags);
    kmm_free(entry);
    return ESP_OK;
}
