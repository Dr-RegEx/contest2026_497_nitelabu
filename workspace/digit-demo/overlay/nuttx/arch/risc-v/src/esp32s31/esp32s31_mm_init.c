/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_mm_init.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <debug.h>

#include <nuttx/compiler.h>
#include <nuttx/cache.h>
#include <nuttx/irq.h>
#include <nuttx/mm/mm.h>
#include <nuttx/spinlock.h>

#include "esp_cache.h"
#include "esp_err.h"
#include "hal/cache_ll.h"
#include "riscv_mmu.h"

#include "esp32s31_mm_init.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Reserve the 1 GiB RISC-V process address-environment window beginning at
 * CONFIG_ARCH_TEXT_VBASE.  The initial 0x61000000 base is also the cached
 * external-memory Sv32 alias already validated by the standalone S31 test.
 * Kernel leaves are deliberately supervisor-only, so copying them into a
 * process cannot expose devices, SRAM, cached Flash, or PSRAM to U-mode.
 */

#define ESP32S31_USER_VBASE       CONFIG_ARCH_TEXT_VBASE
#define ESP32S31_USER_VEND        (ESP32S31_USER_VBASE + 0x40000000u)
#define ESP32S31_SV32_ROOT_ENTRIES 1024

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uintptr_t g_esp32s31_l1_pgtable[ESP32S31_SV32_ROOT_ENTRIES]
  locate_data(".pgtables") aligned_data(4096);

/* Whole-cache maintenance drives global cache-controller state and is not
 * safe to enter from both CPUs at once.  Address-environment switching holds
 * the scheduler's addrenv lock, but ELF loading reaches the same routines
 * without it, so the two paths need a lock of their own.
 */

static spinlock_t g_esp32s31_cache_lock = SP_UNLOCKED;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void esp32s31_invalidate_icaches(void)
{
  /* The S31 cache_ll_invalidate_all() implementation currently ignores its
   * cache type argument and invalidates CACHE_MAP_MASK, which includes the
   * single D-cache shared by both CPUs.  Invalidating that D-cache while the
   * peer CPU is writing a process heap can discard stores made after an
   * earlier writeback.  Address-environment maintenance only needs to remove
   * stale instructions here, so select both L1 I-caches explicitly.
   */

  Cache_Invalidate_All(CACHE_MAP_L1_ICACHE_MASK);
}

static bool esp32s31_sv32_translate(uintptr_t vaddr, uintptr_t *paddr)
{
  uintptr_t root;
  uintptr_t l2;
  uintptr_t pte;

  root = mmu_get_satp_pgbase();
  pte = *(volatile uintptr_t *)(root +
          (((vaddr >> 22) & RV_MMU_VPN_MASK) * sizeof(uintptr_t)));
  if ((pte & PTE_VALID) == 0)
    {
      return false;
    }

  if ((pte & PTE_LEAF_MASK) != 0)
    {
      *paddr = mmu_pte_to_paddr(pte) |
               (vaddr & (RV_MMU_L1_PAGE_SIZE - 1));
      return true;
    }

  l2 = mmu_pte_to_paddr(pte);
  pte = *(volatile uintptr_t *)(l2 +
          (((vaddr >> RV_MMU_PAGE_SHIFT) & RV_MMU_VPN_MASK) *
           sizeof(uintptr_t)));
  if ((pte & PTE_VALID) == 0 || (pte & PTE_LEAF_MASK) == 0)
    {
      return false;
    }

  *paddr = mmu_pte_to_paddr(pte) | (vaddr & RV_MMU_PAGE_MASK);
  return true;
}

static void esp32s31_writeback_user_range(uintptr_t addr, size_t len)
{
  uintptr_t vpage;
  uintptr_t vend;
  uintptr_t ppage;

  if (len == 0 || len > UINTPTR_MAX - addr ||
      addr + len > UINTPTR_MAX - RV_MMU_PAGE_MASK)
    {
      return;
    }

  vpage = addr & ~((uintptr_t)RV_MMU_PAGE_MASK);
  vend = (addr + len + RV_MMU_PAGE_MASK) &
         ~((uintptr_t)RV_MMU_PAGE_MASK);

  while (vpage < vend)
    {
      if (esp32s31_sv32_translate(vpage, &ppage) &&
          ppage >= 0x50000000u && ppage < 0x54000000u)
        {
          esp_cache_msync((void *)ppage, RV_MMU_PAGE_SIZE,
                          ESP_CACHE_MSYNC_FLAG_DIR_C2M |
                          ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
        }

      vpage += RV_MMU_PAGE_SIZE;
    }
}

/****************************************************************************
 * Public Data
 ****************************************************************************/

uintptr_t g_kernel_mappings  = (uintptr_t)g_esp32s31_l1_pgtable;
uintptr_t g_kernel_pgt_pbase = (uintptr_t)g_esp32s31_l1_pgtable;

void esp32s31_dump_mmu_fault(const uintreg_t *regs)
{
  uintptr_t satp = READ_CSR(CSR_SATP);
  uintptr_t root = mmu_satp_to_paddr(satp);
  uintptr_t addr = READ_CSR(CSR_TVAL);
  unsigned int index = (addr >> 22) & RV_MMU_VPN_MASK;
  uintptr_t pte;

  _alert("S31 MMU: satp=%08lx status=%08lx saved=%08lx "
         "kernel=%08lx kernel_pte=%08lx\n",
         (unsigned long)satp, (unsigned long)READ_CSR(CSR_STATUS),
         (unsigned long)regs[REG_INT_CTX],
         (unsigned long)g_kernel_pgt_pbase,
         (unsigned long)g_esp32s31_l1_pgtable[index]);

  /* Page tables are allocated only in internal HP SRAM. Do not turn a
   * corrupt satp into a second fault by dereferencing an arbitrary address.
   */

  if (root < 0x2f000000u || root > 0x2f080000u - RV_MMU_PAGE_SIZE)
    {
      _alert("S31 MMU: root outside internal SRAM: %08lx\n",
             (unsigned long)root);
      return;
    }

  pte = ((volatile uintptr_t *)root)[index];
  _alert("S31 MMU: root=%08lx index=%u pte=%08lx\n",
         (unsigned long)root, index, (unsigned long)pte);
  if ((pte & PTE_VALID) != 0 && (pte & PTE_LEAF_MASK) == 0)
    {
      uintptr_t l2 = mmu_pte_to_paddr(pte);

      if (l2 >= 0x2f000000u && l2 <= 0x2f080000u - RV_MMU_PAGE_SIZE)
        {
          pte = ((volatile uintptr_t *)l2)
                  [(addr >> RV_MMU_PAGE_SHIFT) & RV_MMU_VPN_MASK];
          _alert("S31 MMU: l2=%08lx pte=%08lx\n",
                 (unsigned long)l2, (unsigned long)pte);
        }
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void esp32s31_mm_init(void)
{
  uintptr_t paddr;
  volatile uint32_t *psram_probe = (volatile uint32_t *)0x50000000u;

  memset(g_esp32s31_l1_pgtable, 0, sizeof(g_esp32s31_l1_pgtable));

  /* First-stage bring-up uses 4 MiB identity leaves for all kernel-visible
   * physical addresses except the dedicated 1 GiB U-mode virtual window.
   * Sv32 still enforces the important boundary here: none of these leaves
   * carries PTE_U.  Later hardening can split the broad kernel leaves into
   * 4 KiB W^X mappings without changing the process ABI.
   */

  for (paddr = 0; ; paddr += RV_MMU_L1_PAGE_SIZE)
    {
      if (paddr < ESP32S31_USER_VBASE || paddr >= ESP32S31_USER_VEND)
        {
          mmu_ln_setentry(1, g_kernel_mappings, paddr, paddr,
                          MMU_KDATA_FLAGS | PTE_X);
        }

      if (paddr == UINT32_MAX - RV_MMU_L1_PAGE_SIZE + 1)
        {
          break;
        }
    }

  /* Verify the physical cached-PSRAM path while satp is still Bare.  The
   * page allocator will overwrite this first word before assigning the page.
   */

  *psram_probe = 0x51a7c0deu;

  mmu_enable(g_kernel_pgt_pbase, 0);
}

/****************************************************************************
 * Name: esp32s31_addrenv_coherent
 *
 * Description:
 *   The ELF loader writes process text through the cached physical PSRAM
 *   aperture, then executes it through a different Sv32 virtual alias.  A
 *   RISC-V fence orders those stores but does not clean S31's write-back
 *   D-cache.  Clean it before invalidating the instruction cache so the
 *   first U-mode fetch observes the loaded image rather than old PSRAM data.
 ****************************************************************************/

void esp32s31_addrenv_coherent(void)
{
  irqstate_t flags = spin_lock_irqsave(&g_esp32s31_cache_lock);

  cache_ll_writeback_all(CACHE_LL_LEVEL_ALL, CACHE_TYPE_DATA,
                         CACHE_LL_ID_ALL);
  esp32s31_invalidate_icaches();
  __asm__ volatile ("fence rw, rw\n"
                    "fence.i\n"
                    ::: "memory");

  spin_unlock_irqrestore(&g_esp32s31_cache_lock, flags);
}

/****************************************************************************
 * Name: esp32s31_pgtable_alloc / esp32s31_pgtable_free
 *
 * Description:
 *   Keep Sv32 page-table pages in internal HP SRAM.  Process payload pages
 *   still come from the external PSRAM page pool.  The S31 page-table walker
 *   must not depend on the cached external-memory alias that the tables are
 *   themselves defining.
 ****************************************************************************/

uintptr_t esp32s31_pgtable_alloc(void)
{
  void *vaddr = kmm_memalign(RV_MMU_PAGE_SIZE, RV_MMU_PAGE_SIZE);
  uintptr_t paddr;

  if (vaddr == NULL)
    {
      return 0;
    }

  memset(vaddr, 0, RV_MMU_PAGE_SIZE);
  paddr = (uintptr_t)vaddr - CONFIG_RAM_VSTART + CONFIG_RAM_START;
  return paddr;
}

void esp32s31_pgtable_free(uintptr_t paddr)
{
  kmm_free((void *)(paddr - CONFIG_RAM_START + CONFIG_RAM_VSTART));
}

/****************************************************************************
 * NuttX cache interface
 ****************************************************************************/

#ifdef CONFIG_ARCH_ICACHE
size_t up_get_icache_linesize(void)
{
  return CONFIG_CACHE_L1_CACHE_LINE_SIZE;
}

size_t up_get_icache_size(void)
{
  /* The size is not needed for maintenance: the ROM cache API performs the
   * operation over the complete hardware cache selected by its mask.
   */

  return 0;
}

void up_enable_icache(void)
{
  cache_ll_enable_cache(1, CACHE_TYPE_INSTRUCTION, 0, true, false);
}

void up_disable_icache(void)
{
  cache_ll_disable_cache(1, CACHE_TYPE_INSTRUCTION, 0);
}

void up_invalidate_icache(uintptr_t start, uintptr_t end)
{
  /* Sv32 process addresses are aliases of cached PSRAM physical pages and
   * are not understood by the ROM address-range API.  Invalidating the full
   * I-cache is conservative and correct during process loading.
   */

  irqstate_t flags = spin_lock_irqsave(&g_esp32s31_cache_lock);

  esp32s31_invalidate_icaches();
  __asm__ volatile ("fence.i" ::: "memory");

  spin_unlock_irqrestore(&g_esp32s31_cache_lock, flags);
}

void up_invalidate_icache_all(void)
{
  up_invalidate_icache(0, UINTPTR_MAX);
}

void up_coherent_dcache(uintptr_t addr, size_t len)
{
  /* The ELF loader passes the process Sv32 virtual address here.  The S31
   * cache ROM only recognizes the SoC's cached external-memory apertures,
   * so translate each user page back to its 0x50000000 PSRAM alias before
   * issuing the same address-specific C2M operation used by ESP-IDF.
   */

  /* The S31 address-range cache operation can discard unrelated dirty
   * PSRAM aliases while synchronizing one Sv32 region.  The ELF loader calls
   * this for text before data, so first commit the whole D-cache; otherwise
   * initialized data written immediately before the text sync can be lost.
   * Loading an image is not a hot path, and the full writeback is the
   * conservative coherence boundary needed by these virtual aliases.
   */

  irqstate_t flags = spin_lock_irqsave(&g_esp32s31_cache_lock);

  cache_ll_writeback_all(CACHE_LL_LEVEL_ALL, CACHE_TYPE_DATA,
                         CACHE_LL_ID_ALL);
  esp32s31_writeback_user_range(addr, len);
  esp32s31_invalidate_icaches();
  __asm__ volatile ("fence rw, rw\n"
                    "fence.i\n"
                    ::: "memory");

  spin_unlock_irqrestore(&g_esp32s31_cache_lock, flags);
}
#endif
