/* SPDX-License-Identifier: Apache-2.0
 * Bounded PSRAM page ownership for the optional LittleFS VFS cache.
 */
#include <nuttx/config.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <nuttx/mutex.h>
#include <nuttx/pgalloc.h>

#if !defined(CONFIG_ARCH_CHIP_ESP32S31) || !defined(CONFIG_BUILD_KERNEL) || \
    !defined(CONFIG_ARCH_PGPOOL_MAPPING) || MM_PGSIZE != 4096 || \
    CONFIG_ARCH_PGPOOL_PBASE != CONFIG_ARCH_PGPOOL_VBASE
#  error This allocator requires the reviewed S31 kernel identity-mapped PSRAM page pool
#endif

#define COALESCE_MAX_PAGES 64
static mutex_t g_pages_lock = NXMUTEX_INITIALIZER;
static uintptr_t g_pages[COALESCE_MAX_PAGES];

void *coalesce_page_alloc(void);
int coalesce_page_free(void *buffer);

void *coalesce_page_alloc(void)
{
  uintptr_t page = 0;
  if (nxmutex_lock(&g_pages_lock) < 0)
    {
      return NULL;
    }

  for (unsigned int i = 0; i < COALESCE_MAX_PAGES; i++)
    {
      if (g_pages[i] == 0)
        {
          page = mm_pgalloc(1);
          if (page != 0)
            {
              uintptr_t base = CONFIG_ARCH_PGPOOL_PBASE;
              uintptr_t end = base + CONFIG_ARCH_PGPOOL_SIZE;
              if (page < base || page > end - MM_PGSIZE ||
                  !MM_ISALIGNED(page))
                {
                  mm_pgfree(page, 1);
                  page = 0;
                }
              else
                {
                  /* Kernel identity mapping has no PTE_U. Allocation and
                   * data access occur only in task context with caches on.
                   */

                  memset((void *)page, 0, MM_PGSIZE);
                  g_pages[i] = page;
                }
            }

          break;
        }
    }

  nxmutex_unlock(&g_pages_lock);
  return (void *)page;
}

int coalesce_page_free(void *buffer)
{
  int ret = nxmutex_lock(&g_pages_lock);
  if (ret < 0)
    {
      return ret;
    }

  ret = -1;
  for (unsigned int i = 0; i < COALESCE_MAX_PAGES; i++)
    {
      if (buffer != NULL && g_pages[i] == (uintptr_t)buffer)
        {
          mm_pgfree(g_pages[i], 1);
          g_pages[i] = 0;
          ret = 0;
          break;
        }
    }

  nxmutex_unlock(&g_pages_lock);
  return ret;
}
