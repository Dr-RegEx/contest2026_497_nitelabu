/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_tlb.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Production SMP TLB shootdown.  G.1.3 proved generation/ack + sfence.vma on
 * an isolated CPU1 trap path; this module rebuilds that protocol on top of
 * the NuttX SMP IPI so mapping-side callers can wait with interrupts masked.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <nuttx/arch.h>
#include <nuttx/fs/fs.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>

#include "esp_attr.h"
#include "hal/crosscore_int_ll.h"

#include "esp32s31_cpu1_boot.h"
#include "esp32s31_tlb.h"
#include "riscv_mmu.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ESP32S31_TLB_OP_RANGE   1u
#define ESP32S31_TLB_OP_GLOBAL  2u
#define ESP32S31_TLB_STATS_LEN  128

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct esp32s31_tlb_mailbox_s
{
  volatile uint32_t request_generation;
  volatile uint32_t ack_generation;
  volatile uint32_t operation;
  volatile uintptr_t vaddr;
  volatile size_t size;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct esp32s31_tlb_mailbox_s g_esp32s31_tlb_mbox[CONFIG_SMP_NCPUS];
static spinlock_t g_esp32s31_tlb_lock[CONFIG_SMP_NCPUS];
static volatile uint32_t g_esp32s31_tlb_send_count;
static volatile uint32_t g_esp32s31_tlb_ack_count;
static volatile uint32_t g_esp32s31_tlb_timeout_count;
static volatile uint32_t g_esp32s31_tlb_range_count;
static volatile uint32_t g_esp32s31_tlb_global_count;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t esp32s31_tlb_stats_read(FAR struct file *filep,
                                       FAR char *buffer, size_t buflen);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline void esp32s31_tlb_fence(void)
{
  __asm__ volatile ("fence rw, rw" ::: "memory");
}

static inline void IRAM_ATTR esp32s31_tlb_sfence_vaddr(uintptr_t vaddr)
{
  /* rs1 = vaddr, rs2 = x0: invalidate translations for vaddr in all ASIDs.
   * Do not reuse mmu_invalidate_tlb_by_vaddr(); that helper currently puts
   * the address in the ASID operand.
   */

  __asm__ volatile
    (
      "sfence.vma %0, x0\n"
      :
      : "r" (vaddr)
      : "memory"
    );
}

static inline void IRAM_ATTR esp32s31_tlb_sfence_global(void)
{
  __asm__ volatile ("sfence.vma x0, x0" ::: "memory");
}

static bool esp32s31_tlb_remote_online(int cpu)
{
  if (cpu == 0)
    {
      return true;
    }

  return g_esp32s31_cpu1_boot_state == ESP32S31_CPU1_SMP_ONLINE;
}

static void IRAM_ATTR esp32s31_tlb_apply(uint32_t operation,
                                         uintptr_t vaddr, size_t size)
{
  if (operation == ESP32S31_TLB_OP_GLOBAL)
    {
      esp32s31_tlb_sfence_global();
      g_esp32s31_tlb_global_count++;
    }
  else
    {
      uintptr_t end = vaddr + size;

      for (vaddr &= ~((uintptr_t)RV_MMU_PAGE_MASK); vaddr < end;
           vaddr += RV_MMU_PAGE_SIZE)
        {
          esp32s31_tlb_sfence_vaddr(vaddr);
        }

      g_esp32s31_tlb_range_count++;
    }
}

static void IRAM_ATTR esp32s31_tlb_process_cpu(int cpu)
{
  FAR struct esp32s31_tlb_mailbox_s *mbox = &g_esp32s31_tlb_mbox[cpu];
  uint32_t generation;
  uint32_t operation;
  uintptr_t vaddr;
  size_t size;

  esp32s31_tlb_fence();
  generation = mbox->request_generation;
  if (generation == 0 || generation == mbox->ack_generation)
    {
      return;
    }

  operation = mbox->operation;
  vaddr = mbox->vaddr;
  size = mbox->size;
  esp32s31_tlb_apply(operation, vaddr, size);

  mbox->ack_generation = generation;
  esp32s31_tlb_fence();
  g_esp32s31_tlb_ack_count++;
}

static int IRAM_ATTR esp32s31_tlb_wait_ack(int target, uint32_t generation,
                                           uint32_t timeout_us)
{
  FAR struct esp32s31_tlb_mailbox_s *mbox = &g_esp32s31_tlb_mbox[target];
  int self = this_cpu();

  while (timeout_us-- > 0)
    {
      /* Serve any request aimed at this hart so two CPUs cannot deadlock
       * while each waits for the other with interrupts masked.
       */

      esp32s31_tlb_process_cpu(self);
      esp32s31_tlb_fence();
      if (mbox->ack_generation == generation)
        {
          return OK;
        }

      up_udelay(1);
    }

  return -ETIMEDOUT;
}

static int esp32s31_tlb_submit(int target, uint32_t operation,
                               uintptr_t vaddr, size_t size)
{
  FAR struct esp32s31_tlb_mailbox_s *mbox;
  irqstate_t flags;
  uint32_t generation;
  int ret;

  if (target == this_cpu())
    {
      /* sfence.vma is hart local and idempotent, so the local half needs no
       * mailbox.  Callers rely on it: the page table helpers do not fence.
       */

      esp32s31_tlb_apply(operation, vaddr, size);
      return OK;
    }

  if (!esp32s31_tlb_remote_online(target))
    {
      return OK;
    }

  mbox = &g_esp32s31_tlb_mbox[target];
  flags = spin_lock_irqsave_nopreempt(&g_esp32s31_tlb_lock[target]);

  generation = mbox->request_generation + 1;
  if (generation == 0)
    {
      generation = 1;
    }

  mbox->operation = operation;
  mbox->vaddr = vaddr;
  mbox->size = size;
  mbox->request_generation = generation;
  esp32s31_tlb_fence();
  g_esp32s31_tlb_send_count++;

  crosscore_int_ll_trigger_interrupt(target);
  ret = esp32s31_tlb_wait_ack(target, generation,
                              CONFIG_ESP32S31_SMP_TLB_SHOOTDOWN_TIMEOUT_US);
  if (ret == -ETIMEDOUT)
    {
      g_esp32s31_tlb_timeout_count++;
      esp32s31_tlb_sfence_global();
    }

  spin_unlock_irqrestore_nopreempt(&g_esp32s31_tlb_lock[target], flags);
  return ret;
}

static ssize_t esp32s31_tlb_stats_read(FAR struct file *filep,
                                       FAR char *buffer, size_t buflen)
{
  char line[ESP32S31_TLB_STATS_LEN];
  size_t len;
  size_t ncopy;

  len = snprintf(line, sizeof(line),
                 "TLBSHOOT: send=%" PRIu32 " ack=%" PRIu32
                 " timeout=%" PRIu32 " range=%" PRIu32
                 " global=%" PRIu32 "\n",
                 g_esp32s31_tlb_send_count,
                 g_esp32s31_tlb_ack_count,
                 g_esp32s31_tlb_timeout_count,
                 g_esp32s31_tlb_range_count,
                 g_esp32s31_tlb_global_count);

  if (filep->f_pos >= (off_t)len)
    {
      return 0;
    }

  ncopy = len - (size_t)filep->f_pos;
  if (ncopy > buflen)
    {
      ncopy = buflen;
    }

  memcpy(buffer, line + filep->f_pos, ncopy);
  filep->f_pos += (off_t)ncopy;
  return (ssize_t)ncopy;
}

static const struct file_operations g_esp32s31_tlb_stats_ops =
{
  NULL,                    /* open */
  NULL,                    /* close */
  esp32s31_tlb_stats_read, /* read */
  NULL,                    /* write */
  NULL,                    /* seek */
  NULL,                    /* ioctl */
  NULL,                    /* mmap */
  NULL,                    /* truncate */
  NULL,                    /* poll */
  NULL,                    /* readv */
  NULL                     /* writev */
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void IRAM_ATTR esp32s31_tlb_shootdown_process(void)
{
  esp32s31_tlb_process_cpu(this_cpu());
}

int riscv_tlb_shootdown_range(uintptr_t vaddr, size_t size)
{
  int cpu;
  int ret = OK;
  int rc;

  if (size == 0)
    {
      return riscv_tlb_shootdown_all();
    }

  for (cpu = 0; cpu < CONFIG_SMP_NCPUS; cpu++)
    {
      rc = esp32s31_tlb_submit(cpu, ESP32S31_TLB_OP_RANGE, vaddr, size);
      if (rc < 0 && ret >= 0)
        {
          ret = rc;
        }
    }

  return ret;
}

int riscv_tlb_shootdown_all(void)
{
  int cpu;
  int ret = OK;
  int rc;

  for (cpu = 0; cpu < CONFIG_SMP_NCPUS; cpu++)
    {
      rc = esp32s31_tlb_submit(cpu, ESP32S31_TLB_OP_GLOBAL, 0, 0);
      if (rc < 0 && ret >= 0)
        {
          ret = rc;
        }
    }

  return ret;
}

int esp32s31_tlb_register_stats_device(void)
{
  return register_driver("/dev/tlbshoot", &g_esp32s31_tlb_stats_ops,
                         0444, NULL);
}
