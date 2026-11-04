/* SPDX-License-Identifier: Apache-2.0 */
#include <nuttx/config.h>
#include <nuttx/pgalloc.h>
#include <stdint.h>
#include <stddef.h>
#include <debug.h>

/* Keep an entire metadata page before the aligned DMA span. Page ownership
 * is exclusive to this allocation; descriptors remain in internal SRAM. */
#define MAGIC 0x5333444d
void *s31vision_dma_alloc(size_t alignment, size_t size)
{
  if (!size || alignment > MM_PGSIZE || (alignment & (alignment - 1)) ||
      size > SIZE_MAX - 2 * MM_PGSIZE) return NULL;
  unsigned pages = MM_NPAGES(size) + 1;
  uintptr_t base = mm_pgalloc(pages);
  if (!base) return NULL;
  uint32_t *meta = (uint32_t *)base;
  meta[0] = MAGIC; meta[1] = pages;
  return (void *)(base + MM_PGSIZE);
}
void s31vision_dma_free(void *ptr)
{
  if (!ptr) return;
  uint32_t *meta = (uint32_t *)((uintptr_t)ptr - MM_PGSIZE);
  DEBUGASSERT(MM_ISALIGNED(ptr) && meta[0] == MAGIC);
  unsigned pages = meta[1]; meta[0] = 0;
  mm_pgfree((uintptr_t)meta, pages);
}
