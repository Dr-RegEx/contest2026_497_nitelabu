/****************************************************************************
 * arch/risc-v/src/common/espressif/esp_start.c
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

#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/init.h>
#include <nuttx/mm/kasan.h>

#include "riscv_internal.h"

#include "esp_irq.h"
#include "esp_libc_stubs.h"
#include "esp_lowputc.h"
#include "esp_start.h"

#include "esp_clk_internal.h"
#include "esp_private/rtc_clk.h"
#include "esp_cpu.h"
#include "esp_private/brownout.h"
#include "hal/wdt_hal.h"
#include "hal/mmu_hal.h"
#include "hal/mmu_types.h"
#include "hal/cache_types.h"
#include "hal/cache_ll.h"
#include "hal/cache_hal.h"
#include "soc/ext_mem_defs.h"
#ifndef CONFIG_ARCH_CHIP_ESP32S31
#include "soc/extmem_reg.h"
#include "soc/mmu.h"
#endif
#include "soc/reg_base.h"
#ifdef CONFIG_ARCH_CHIP_ESP32S31
#  include "soc/soc.h"
#  include "soc/soc_caps.h"
#  include "riscv/csr.h"
#  ifdef CONFIG_ARCH_USE_S_MODE
#    include "soc/cpu_apm_reg.h"
#    include "soc/tee_reg.h"
#    ifdef CONFIG_BUILD_KERNEL
#      include "esp32s31_mm_init.h"
#    endif
#  endif
#endif
#include "spi_flash_mmap.h"
#include "rom/cache.h"

#include "bootloader_init.h"

#ifdef CONFIG_ARCH_CHIP_ESP32S31
#  include "bootloader_mem.h"
#  include "esp_private/esp_mmu_map_private.h"
#  include "esp_private/spi_flash_os.h"
#  ifdef CONFIG_ESPRESSIF_SPIRAM
#    include "esp_psram.h"
#    include "esp_private/esp_psram_extram.h"
#  endif
#  ifdef CONFIG_ESP32S31_SMP
#    include "../../esp32s31/esp32s31_cpu1_boot.h"
#  endif
#endif

#ifdef CONFIG_ESPRESSIF_SIMPLE_BOOT
#include "bootloader_flash_priv.h"
#  ifdef CONFIG_ARCH_CHIP_ESP32S31
#  include "esp_rom_serial_output.h"
#  else
#    include "esp_rom_uart.h"
#  endif
#include "esp_rom_sys.h"
#include "esp_app_format.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_DEBUG_FEATURES
#  define showprogress(c)     riscv_lowputc(c)
#else
#  define showprogress(c)
#endif

#if defined(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT) || \
    defined (CONFIG_ESPRESSIF_SIMPLE_BOOT)
#ifdef CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT
#  define PRIMARY_SLOT_OFFSET   CONFIG_ESPRESSIF_OTA_PRIMARY_SLOT_OFFSET
#  define MMU_FLASH_MASK        (~(MMU_BLOCK_SIZE - 1))
#else
/* Force offset to the beginning of the whole image
 */

#  define PRIMARY_SLOT_OFFSET   0
#endif
#  define HDR_ATTR              __attribute__((section(".entry_addr"))) \
                                  __attribute__((used))
#ifndef CONFIG_ARCH_CHIP_ESP32S31
#  define FLASH_MMU_TABLE       ((volatile uint32_t*) DR_REG_MMU_TABLE)
#  define FLASH_MMU_TABLE_SIZE  (ICACHE_MMU_SIZE/sizeof(uint32_t))
#  define MMU_BLOCK_SIZE        0x00010000  /* 64 KB */
#  define CACHE_REG             EXTMEM_ICACHE_CTRL1_REG
#  define CACHE_MASK            (EXTMEM_ICACHE_SHUT_IBUS_M | \
                                 EXTMEM_ICACHE_SHUT_DBUS_M)
#endif

#  define CHECKSUM_ALIGN        16
#  define IS_PADD(addr) ((addr) == 0)
#  define IS_DRAM(addr) ((addr) >= SOC_DRAM_LOW && (addr) < SOC_DRAM_HIGH)
#  define IS_IRAM(addr) ((addr) >= SOC_IRAM_LOW && (addr) < SOC_IRAM_HIGH)
#  define IS_IROM(addr) ((addr) >= SOC_IROM_LOW && (addr) < SOC_IROM_HIGH)
#  define IS_DROM(addr) ((addr) >= SOC_DROM_LOW && (addr) < SOC_DROM_HIGH)
#  define IS_SRAM(addr) (IS_IRAM(addr) || IS_DRAM(addr))
#  define IS_MMAP(addr) (IS_IROM(addr) || IS_DROM(addr))
#  ifdef SOC_RTC_FAST_MEM_SUPPORTED
#    define IS_RTC_FAST_IRAM(addr) \
                        ((addr) >= SOC_RTC_IRAM_LOW \
                         && (addr) < SOC_RTC_IRAM_HIGH)
#    define IS_RTC_FAST_DRAM(addr) \
                        ((addr) >= SOC_RTC_DRAM_LOW \
                         && (addr) < SOC_RTC_DRAM_HIGH)
#  else
#    define IS_RTC_FAST_IRAM(addr) false
#    define IS_RTC_FAST_DRAM(addr) false
#  endif
#  ifdef SOC_RTC_SLOW_MEM_SUPPORTED
#    define IS_RTC_SLOW_DRAM(addr) \
                        ((addr) >= SOC_RTC_DATA_LOW \
                         && (addr) < SOC_RTC_DATA_HIGH)
#  else
#    define IS_RTC_SLOW_DRAM(addr) false
#  endif
#  define IS_NONE(addr) (!IS_IROM(addr) \
                         && !IS_DROM(addr) \
                         && !IS_IRAM(addr) \
                         && !IS_DRAM(addr) \
                         && !IS_RTC_FAST_IRAM(addr) \
                         && !IS_RTC_FAST_DRAM(addr) \
                         && !IS_RTC_SLOW_DRAM(addr) \
                         && !IS_PADD(addr))

#  define IS_MAPPING(addr) IS_IROM(addr) || IS_DROM(addr)
#endif

#if defined(CONFIG_ARCH_CHIP_ESP32S31) && \
    defined(CONFIG_ARCH_USE_S_MODE)
#  define ESP32S31_CPU_APM_REE0_RWX 0x7
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

#if defined(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT) || \
    defined (CONFIG_ESPRESSIF_SIMPLE_BOOT)
extern uint8_t _image_irom_vma[];
extern uint8_t _image_irom_lma[];
extern uint8_t _image_irom_size[];

extern uint8_t _image_drom_vma[];
extern uint8_t _image_drom_lma[];
extern uint8_t _image_drom_size[];
#endif

#ifdef CONFIG_ARCH_CHIP_ESP32S31
extern int _vector_table;
extern int _mtvt_table;
#endif

/****************************************************************************
 * ROM Function Prototypes
 ****************************************************************************/

#if defined(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT) || \
    defined (CONFIG_ESPRESSIF_SIMPLE_BOOT)
extern int ets_printf(const char *fmt, ...) printf_like(1, 2);
#endif

extern void cache_set_idrom_mmu_size(uint32_t irom_size, uint32_t drom_size);

#if defined(CONFIG_ARCH_CHIP_ESP32S31) && \
    defined(CONFIG_ARCH_USE_S_MODE)
extern noreturn_function void esp32s31_start_smode(void (*entry)(void));
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#if defined(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT) || \
    defined (CONFIG_ESPRESSIF_SIMPLE_BOOT)
IRAM_ATTR noreturn_function void __start(void);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

#if defined(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT) || \
    defined (CONFIG_ESPRESSIF_SIMPLE_BOOT)
HDR_ATTR static void (*_entry_point)(void) = __start;
#endif

/****************************************************************************
 * Public Data
 ****************************************************************************/

extern uint8_t _instruction_reserved_start[];
extern uint8_t _instruction_reserved_end[];
extern uint8_t _rodata_reserved_start[];
extern uint8_t _rodata_reserved_end[];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: calc_mmu_pages
 *
 * Description:
 *   Calculate the number of cache pages to map.
 *
 * Input Parameters:
 *   size  - Size of data to map
 *   vaddr - Virtual address where data will be mapped
 *
 * Returned Value:
 *   Number of cache MMU pages required to do the mapping.
 *
 ****************************************************************************/

#if defined(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT) || \
    defined (CONFIG_ESPRESSIF_SIMPLE_BOOT)
#ifndef CONFIG_ARCH_CHIP_ESP32S31
static inline uint32_t calc_mmu_pages(uint32_t size, uint32_t vaddr)
{
  return (size + (vaddr - (vaddr & MMU_FLASH_MASK)) + MMU_BLOCK_SIZE - 1) /
    MMU_BLOCK_SIZE;
}
#endif
#endif

/****************************************************************************
 * Name: map_rom_segments
 *
 * Description:
 *   Configure the MMU and Cache peripherals for accessing ROM code and data.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

#if defined(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT) || \
    defined(CONFIG_ESPRESSIF_SIMPLE_BOOT)
static int map_rom_segments(uint32_t app_drom_start, uint32_t app_drom_vaddr,
                            uint32_t app_drom_size, uint32_t app_irom_start,
                            uint32_t app_irom_vaddr, uint32_t app_irom_size)
{
  uint32_t rc = 0;
  uint32_t actual_mapped_len = 0;
  uint32_t app_irom_start_aligned = app_irom_start & MMU_FLASH_MASK;
  uint32_t app_irom_vaddr_aligned = app_irom_vaddr & MMU_FLASH_MASK;
  uint32_t app_drom_start_aligned = app_drom_start & MMU_FLASH_MASK;
  uint32_t app_drom_vaddr_aligned = app_drom_vaddr & MMU_FLASH_MASK;
#ifdef CONFIG_ARCH_CHIP_ESP32S31
  uint32_t app_irom_map_size;
  uint32_t app_drom_map_size;
#endif
#ifdef CONFIG_ESPRESSIF_SIMPLE_BOOT
  esp_image_header_t image_header; /* Header for entire image */
  esp_image_segment_header_t WORD_ALIGNED_ATTR segment_hdr;
  bool padding_checksum = false;
  unsigned int segments = 0;
  unsigned int ram_segments = 0;
  unsigned int rom_segments = 0;
  size_t offset = CONFIG_BOOTLOADER_OFFSET_IN_FLASH;
#endif

#ifdef CONFIG_ESPRESSIF_SIMPLE_BOOT

  /* Read image header */

  if (bootloader_flash_read(offset, &image_header,
                            sizeof(esp_image_header_t),
                            true) != ESP_OK)
    {
      ets_printf("Failed to load image header!\n");
      abort();
    }

  offset += sizeof(esp_image_header_t);

  /* Iterate for segment information parsing */

  while (segments++ < 16 && rom_segments < 2)
    {
      /* Read segment header */

      if (bootloader_flash_read(offset, &segment_hdr,
                                sizeof(esp_image_segment_header_t),
                                true) != ESP_OK)
        {
          ets_printf("failed to read segment header at %x\n", offset);
          abort();
        }

      if (IS_NONE(segment_hdr.load_addr))
        {
          /* Total segment count = (segments - 1) */

          break;
        }

      if (IS_RTC_FAST_IRAM(segment_hdr.load_addr) ||
          IS_RTC_FAST_DRAM(segment_hdr.load_addr) ||
          IS_RTC_SLOW_DRAM(segment_hdr.load_addr))
        {
          /* RTC segment is loaded by ROM bootloader */

          ram_segments++;
        }

      ets_printf("%s: lma 0x%08x vma 0x%08lx len 0x%-6lx (%lu)\n",
          IS_NONE(segment_hdr.load_addr) ? "???" :
            IS_RTC_FAST_IRAM(segment_hdr.load_addr) ||
            IS_RTC_FAST_DRAM(segment_hdr.load_addr) ||
            IS_RTC_SLOW_DRAM(segment_hdr.load_addr) ? "rtc" :
              IS_MMAP(segment_hdr.load_addr) ?
                IS_IROM(segment_hdr.load_addr) ? "imap" : "dmap" :
                  IS_PADD(segment_hdr.load_addr) ? "padd" :
                    IS_DRAM(segment_hdr.load_addr) ? "dram" : "iram",
          offset + sizeof(esp_image_segment_header_t),
          segment_hdr.load_addr, segment_hdr.data_len,
          segment_hdr.data_len);

      /* Fix drom and irom produced be the linker, as this
       * is later invalidated by the elf2image command.
       */

      if (IS_DROM(segment_hdr.load_addr) &&
          segment_hdr.load_addr == (uint32_t)_image_drom_vma)
        {
          app_drom_start = offset + sizeof(esp_image_segment_header_t);
          app_drom_start_aligned = app_drom_start & MMU_FLASH_MASK;
          rom_segments++;
        }

      if (IS_IROM(segment_hdr.load_addr) &&
          segment_hdr.load_addr == (uint32_t)_image_irom_vma)
        {
          app_irom_start = offset + sizeof(esp_image_segment_header_t);
          app_irom_start_aligned = app_irom_start & MMU_FLASH_MASK;
          rom_segments++;
        }

      if (IS_SRAM(segment_hdr.load_addr))
        {
          ram_segments++;
        }

      offset += sizeof(esp_image_segment_header_t) + segment_hdr.data_len;
      if (ram_segments == image_header.segment_count && !padding_checksum)
        {
          offset += (CHECKSUM_ALIGN - 1) - (offset % CHECKSUM_ALIGN) + 1;
#ifdef CONFIG_ARCH_CHIP_ESP32S31
          if (image_header.hash_appended)
            {
              offset += ESP_IMAGE_HASH_LEN;
            }
#endif
          padding_checksum = true;
        }
    }

  if (segments == 0 || segments == 16)
    {
      ets_printf("Error parsing segments\n");
    }

  ets_printf("total segments stored %d\n", segments - 1);
#endif

#ifdef CONFIG_ARCH_CHIP_ESP32S31
  cache_hal_disable(CACHE_LL_LEVEL_EXT_MEM, CACHE_TYPE_ALL);

  app_irom_map_size = app_irom_size +
                      (app_irom_vaddr - app_irom_vaddr_aligned);
  app_drom_map_size = app_drom_size +
                      (app_drom_vaddr - app_drom_vaddr_aligned);
#else
  cache_hal_disable(CACHE_TYPE_ALL);
#endif

  /* Clear the MMU entries that are already set up,
   * so the new app only has the mappings it creates.
   */

  mmu_hal_unmap_all();

  mmu_hal_map_region(0, MMU_TARGET_FLASH0,
                     app_drom_vaddr_aligned, app_drom_start_aligned,
#ifdef CONFIG_ARCH_CHIP_ESP32S31
                     app_drom_map_size, &actual_mapped_len);
#else
                     app_drom_size, &actual_mapped_len);
#endif

  mmu_hal_map_region(0, MMU_TARGET_FLASH0,
                     app_irom_vaddr_aligned, app_irom_start_aligned,
#ifdef CONFIG_ARCH_CHIP_ESP32S31
                     app_irom_map_size, &actual_mapped_len);
#else
                     app_irom_size, &actual_mapped_len);
#endif

  /* ------------------Enable corresponding buses--------------------- */

  cache_bus_mask_t bus_mask = cache_ll_l1_get_bus(0, app_drom_vaddr_aligned,
#ifdef CONFIG_ARCH_CHIP_ESP32S31
                                                  app_drom_map_size);
#else
                                                  app_drom_size);
#endif
  cache_ll_l1_enable_bus(0, bus_mask);
#ifdef CONFIG_ARCH_CHIP_ESP32S31
  bus_mask = cache_ll_l1_get_bus(0, app_irom_vaddr_aligned,
                                 app_irom_map_size);
#else
  bus_mask = cache_ll_l1_get_bus(0, app_irom_vaddr_aligned, app_irom_size);
#endif
  cache_ll_l1_enable_bus(0, bus_mask);
#if CONFIG_ESPRESSIF_NUM_CPUS > 1
#ifdef CONFIG_ARCH_CHIP_ESP32S31
  bus_mask = cache_ll_l1_get_bus(1, app_drom_vaddr_aligned,
                                 app_drom_map_size);
#else
  bus_mask = cache_ll_l1_get_bus(1, app_drom_vaddr_aligned, app_drom_size);
#endif
  cache_ll_l1_enable_bus(1, bus_mask);
#ifdef CONFIG_ARCH_CHIP_ESP32S31
  bus_mask = cache_ll_l1_get_bus(1, app_irom_vaddr_aligned,
                                 app_irom_map_size);
#else
  bus_mask = cache_ll_l1_get_bus(1, app_irom_vaddr_aligned, app_irom_size);
#endif
  cache_ll_l1_enable_bus(1, bus_mask);
#endif

#if defined(CONFIG_ARCH_CHIP_ESP32S31) && \
    SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
  cache_ll_invalidate_addr(CACHE_LL_LEVEL_ALL, CACHE_TYPE_ALL,
                           CACHE_LL_ID_ALL, app_irom_vaddr_aligned,
                           actual_mapped_len);
#endif

  /* ------------------Enable Cache----------------------------------- */

#ifdef CONFIG_ARCH_CHIP_ESP32S31
  cache_hal_enable(CACHE_LL_LEVEL_EXT_MEM, CACHE_TYPE_ALL);
#else
  cache_hal_enable(CACHE_TYPE_ALL);
#endif

  return (int)rc;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#if defined(CONFIG_ARCH_CHIP_ESP32S31) && \
    defined(CONFIG_ARCH_USE_S_MODE)

/****************************************************************************
 * Name: esp32s31_mmode_fault
 ****************************************************************************/

noreturn_function void esp32s31_mmode_fault(uint32_t cause, uint32_t epc,
                                             uint32_t tval,
                                             const uint32_t *regs)
{
  uint32_t status;
  uint32_t hart;
  uint32_t fcsr = UINT32_MAX;

  __asm__ volatile ("csrr %0, mstatus" : "=r" (status));
  __asm__ volatile ("csrr %0, mhartid" : "=r" (hart));
#ifdef CONFIG_ARCH_FPU
  if ((status & MSTATUS_FS) != 0)
    {
      __asm__ volatile ("frcsr %0" : "=r" (fcsr));
    }
#endif

  ets_printf("\nS31SM:M-TRAP mcause=0x%08" PRIx32
             " mepc=0x%08" PRIx32 " mtval=0x%08" PRIx32 "\n",
             cause, epc, tval);
  ets_printf("S31SM:FAULT-REGS a0=0x%08" PRIx32
             " a1=0x%08" PRIx32 " ra=0x%08" PRIx32
             " sp=0x%08" PRIx32 "\n",
             regs[0], regs[1], regs[8], regs[9]);
  ets_printf("S31SM:FAULT-FPU hart=%" PRIu32 " mstatus=0x%08" PRIx32
             " fcsr=0x%08" PRIx32 "\n", hart, status, fcsr);

  for (; ; )
    {
      __asm__ volatile ("wfi");
    }
}

/****************************************************************************
 * Name: esp32s31_prepare_hart_smode
 ****************************************************************************/

void IRAM_ATTR esp32s31_prepare_hart_smode(void)
{
  /* Replace ROM's combined Flash/PSRAM PMA with separate executable Flash
   * and read/write/execute PSRAM apertures on each hart.  Sv32 applies the
   * finer per-page user and W^X permissions after entering S-mode.
   */

  PMA_ENTRY_CFG_RESET(15);
  PMA_RESET_AND_ENTRY_SET_NAPOT(6, SOC_IROM_LOW,
                                SOC_IROM_HIGH - SOC_IROM_LOW,
                                PMA_NAPOT | PMA_L | PMA_EN |
                                PMA_R | PMA_X);
  PMA_RESET_AND_ENTRY_SET_NAPOT(7, SOC_EXTRAM_LOW, SOC_EXTRAM_SIZE,
                                PMA_NAPOT | PMA_L | PMA_EN |
                                PMA_R | PMA_W | PMA_X);
  __asm__ volatile ("fence rw, rw" ::: "memory");
  __asm__ volatile ("fence.i" ::: "memory");
}

static void esp32s31_prepare_smode(void)
{
  uint32_t tee_supervisor;
  uint32_t apm_region0;

#ifdef CONFIG_ESP32S31_SMP
  /* CPU1 is bus master M1.  Select the same REE class used by CPU0 before
   * releasing it; supervisor_priv_sel still separates S-mode from U-mode.
   */

  REG_CLR_BIT(TEE_M1_MODE_CTRL_REG, TEE_M1_MODE_M);
#endif

  tee_supervisor = REG_READ(TEE_SUPERVISOR_PRIV_SEL_REG);
#ifdef CONFIG_ESP32S31_SMP
  tee_supervisor |= TEE_CORE1_SUPERVISOR_PRIV_SEL;
#endif
  REG_WRITE(TEE_SUPERVISOR_PRIV_SEL_REG,
            tee_supervisor | TEE_CORE0_SUPERVISOR_PRIV_SEL);

  apm_region0 = REG_READ(CPU_APM_REGION0_ATTR_REG);
  REG_WRITE(CPU_APM_REGION0_ATTR_REG,
            apm_region0 | ESP32S31_CPU_APM_REE0_RWX);

  esp32s31_prepare_hart_smode();
}

void esp32s31_smode_irq_drop(int cpuint)
{
  register uint32_t service __asm__("a0") = 4;
  register uint32_t source __asm__("a1") = (uint32_t)cpuint;

  __asm__ volatile ("ecall"
                    : "+r"(service), "+r"(source)
                    :
                    : "a2", "a3", "a4", "a5", "a6", "a7",
                      "t0", "t1", "t2", "t3", "t4", "t5", "t6",
                      "memory");
}

static void esp32s31_nx_start_smode(void)
{
#ifdef CONFIG_BUILD_KERNEL
  esp32s31_mm_init();
#endif

  nx_start();
}

#endif

/****************************************************************************
 * Name: __esp_start
 ****************************************************************************/

nosanitize_address void __esp_start(void)
{
#if defined(CONFIG_ARCH_CHIP_ESP32S31) && defined(CONFIG_MM_KASAN)
  /* A warm reset can retain the previous KASAN marker in SRAM.  Stop the
   * checker before clearing BSS; heap registration will enable it again.
   */

  kasan_stop();
#endif
#ifdef CONFIG_ARCH_CHIP_ESP32S31
  int ret;
#endif
#if defined(CONFIG_ARCH_CHIP_ESP32S31) && \
    defined(CONFIG_ARCH_USE_S_MODE) && defined(CONFIG_ESP32S31_SMP)
  uintptr_t cpu0_percpu;
#endif
#ifdef CONFIG_ARCH_CHIP_ESP32S31
  /* A flat build runs in M-mode and uses the common vector directly.  In an
   * S-mode build, esp_head.S has already installed the machine monitor;
   * replacing mtvec/mtvt with the supervisor tables here would make any
   * early M-mode fault dereference sscratch before S-mode exists.
   */

#  ifndef CONFIG_ARCH_USE_S_MODE
  esp_cpu_intr_set_ivt_addr(&_vector_table);
  esp_cpu_intr_set_mtvt_addr(&_mtvt_table);
#  endif

  /* The ROM loads the image's RAM segments but does not initialize BSS.
   * ESP32-S31 bootloader_init() uses static state, so clear BSS before
   * entering it.
   */

  bootloader_clear_bss_section();
#endif

#ifdef CONFIG_ESP_ROM_NEEDS_SET_CACHE_MMU_SIZE
  uint32_t _instruction_size;
  uint32_t instruction_page_offset;
  uint32_t cache_mmu_irom_size;
#endif

#if defined(CONFIG_ARCH_CHIP_ESP32S31) && \
    defined(CONFIG_ARCH_USE_S_MODE) && defined(CONFIG_ESP32S31_SMP) && \
    defined(CONFIG_RISCV_PERCPU_SCRATCH)
  /* SMP-aware HAL locks query this_cpu() during bootloader initialization. */

  riscv_percpu_add_hart(0);
  cpu0_percpu = READ_CSR(CSR_SCRATCH);
#endif

#ifdef CONFIG_ESPRESSIF_SIMPLE_BOOT
#  ifdef CONFIG_ARCH_CHIP_ESP32S31
  ret = bootloader_init();
  if (ret != 0)
    {
      ets_printf("Hardware init failed, aborting: %d (0x%x)\n", ret, ret);
      while (true);
    }
#  else
  if (bootloader_init() != 0)
    {
      ets_printf("Hardware init failed, aborting\n");
      while (true);
    }
#  endif

#else
#ifndef CONFIG_ARCH_CHIP_ESP32S31
  bootloader_clear_bss_section();
#endif
#endif

#if defined(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT) || \
    defined(CONFIG_ESPRESSIF_SIMPLE_BOOT)
  size_t partition_offset = PRIMARY_SLOT_OFFSET;
  uint32_t app_irom_start = partition_offset + (uint32_t)_image_irom_lma;
  uint32_t app_irom_size  = (uint32_t)_image_irom_size;
  uint32_t app_irom_vaddr = (uint32_t)_image_irom_vma;
  uint32_t app_drom_start = partition_offset + (uint32_t)_image_drom_lma;
  uint32_t app_drom_size  = (uint32_t)_image_drom_size;
  uint32_t app_drom_vaddr = (uint32_t)_image_drom_vma;

  if (map_rom_segments(app_drom_start, app_drom_vaddr, app_drom_size,
                       app_irom_start, app_irom_vaddr, app_irom_size) != 0)
    {
      ets_printf("Failed to setup XIP, aborting\n");
      while (true);
    }

#endif

#if CONFIG_ESP_ROM_NEEDS_SET_CACHE_MMU_SIZE
  _instruction_size = (uint32_t)&_instruction_reserved_end - \
                      (uint32_t)&_instruction_reserved_start;
  instruction_page_offset =
      (uint32_t)&_instruction_reserved_start &
      (SPI_FLASH_MMU_PAGE_SIZE - 1);
  cache_mmu_irom_size =
      ((instruction_page_offset + _instruction_size +
        SPI_FLASH_MMU_PAGE_SIZE - 1) / \
      SPI_FLASH_MMU_PAGE_SIZE) * sizeof(uint32_t);

  /* Configure the Cache MMU size for instruction and rodata in flash. */

  cache_set_idrom_mmu_size(cache_mmu_irom_size,
                           CACHE_DROM_MMU_MAX_END - cache_mmu_irom_size);
#endif /* CONFIG_ESP_ROM_NEEDS_SET_CACHE_MMU_SIZE */

#if CONFIG_ESP_SYSTEM_BBPLL_RECALIB
  rtc_clk_recalib_bbpll();
#endif

#ifdef CONFIG_ESPRESSIF_REGION_PROTECTION
  /* Configure region protection */

  esp_cpu_configure_region_protection();
#endif

#ifdef CONFIG_ARCH_CHIP_ESP32S31
  /* Complete the application-side memory and flash setup.  Simple Boot's
   * bootloader initialization establishes the ROM loader environment, but
   * these steps publish that state to the ESP-IDF runtime used by NuttX.
   */

  esp_rtc_init();
  esp_mspi_pin_init();
  spi_flash_init_chip_state();
  esp_mmu_map_init();

#if CONFIG_ESP_ROM_NEEDS_SET_CACHE_MMU_SIZE
  /* esp_mmu_map_init() repeats ESP-IDF's size-only calculation.  Restore
   * the page-offset-aware instruction mapping required by NuttX's IROM.
   */

  cache_set_idrom_mmu_size(cache_mmu_irom_size,
                           CACHE_DROM_MMU_MAX_END - cache_mmu_irom_size);
#endif

#ifdef CONFIG_ESPRESSIF_SPIRAM
  ret = esp_psram_chip_init();
  if (ret != ESP_OK)
    {
#  ifndef CONFIG_ESPRESSIF_SPIRAM_IGNORE_NOTFOUND
      PANIC();
#  endif
    }

#  ifdef CONFIG_ESPRESSIF_SPIRAM_BOOT_INIT
  if (ret == ESP_OK)
    {
      ret = esp_psram_init();
      if (ret != ESP_OK)
        {
#    ifndef CONFIG_ESPRESSIF_SPIRAM_IGNORE_NOTFOUND
          PANIC();
#    endif
        }
    }
#  endif
#endif
#endif

  /* Configures the CPU clock, RTC slow and fast clocks, and performs
   * RTC slow clock calibration.
   */

  esp_clk_init();

#ifdef CONFIG_ARCH_CHIP_ESP32S31
  esp_mspi_pin_reserve();
  bootloader_init_mem();
#endif

  /* Disable clock of unused peripherals */

  esp_perip_clk_init();

#ifdef CONFIG_ESPRESSIF_BROWNOUT_DET
  /* Initialize hardware brownout check and reset */

  esp_brownout_init();
#endif

  /* Configure the UART so we can get debug output */

#ifndef CONFIG_ARCH_CHIP_ESP32S31
  esp_lowsetup();
#endif

#ifdef USE_EARLYSERIALINIT
  /* Perform early serial initialization */

#ifndef CONFIG_ARCH_CHIP_ESP32S31
  riscv_earlyserialinit();
#endif
#endif

  showprogress('A');

  /* Setup the syscall table needed by the ROM code */

  esp_setup_syscall_table();

  showprogress('B');

  /* The 2nd stage bootloader enables RTC WDT to monitor any issues that may
   * prevent the startup sequence from finishing correctly. Hence disable it
   * as NuttX is about to start.
   */

  wdt_hal_context_t rwdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
  wdt_hal_write_protect_disable(&rwdt_ctx);
  wdt_hal_set_flashboot_en(&rwdt_ctx, false);
  wdt_hal_disable(&rwdt_ctx);
  wdt_hal_write_protect_enable(&rwdt_ctx);

  /* Initialize onboard resources */

  esp_board_initialize();

  showprogress('C');

#if defined(CONFIG_ARCH_CHIP_ESP32S31) && \
  defined(CONFIG_ARCH_USE_S_MODE)
  esp32s31_prepare_smode();
#  ifdef CONFIG_ESP32S31_SMP
  {
    uint32_t cpu1_hartid;
    uint32_t cpu1_state;

    ret = esp32s31_cpu1_boot(&cpu1_hartid, &cpu1_state);
    if (ret < 0)
      {
        ets_printf("\nCPU1 bring-up failed: ret=%d hart=%u state=0x%08x\n",
                   ret, (unsigned int)cpu1_hartid,
                   (unsigned int)cpu1_state);
        PANIC();
      }
  }
#  endif
#  ifdef CONFIG_RISCV_PERCPU_SCRATCH
#    ifdef CONFIG_ESP32S31_SMP
  WRITE_CSR(CSR_SCRATCH, cpu0_percpu);
  UP_DSB();
#    else
  riscv_percpu_add_hart(0);
#    endif
#  endif
  esp32s31_start_smode(esp32s31_nx_start_smode);
#elif defined(CONFIG_ARCH_CHIP_ESP32S31) && defined(CONFIG_ARCH_FPU)
  /* Flat-mode startup does not pass through the supervisor entry code that
   * normally enables the FPU.  Enable it before the first interrupt saves a
   * task context containing floating-point registers.
   */

  riscv_fpuconfig();
#endif

  /* Bring up NuttX */

  nx_start();

  for (; ; );
}
