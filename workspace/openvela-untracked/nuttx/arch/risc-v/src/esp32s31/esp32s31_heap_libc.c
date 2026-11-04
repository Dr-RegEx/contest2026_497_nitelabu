/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_heap_libc.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/kmalloc.h>

/* ESP-IDF permits ordinary free/realloc on capability allocations. Preserve
 * that contract with separate SRAM and PSRAM heaps in a FLAT build. Explicit
 * kmm_free remains strict; this does not hide a mismatched kernel owner.
 */

void __real_free(void *ptr);
void *__real_realloc(void *ptr, size_t size);
size_t __real_malloc_size(void *ptr);

void __wrap_free(void *ptr)
{
  if (ptr != NULL && kmm_heapmember(ptr))
    {
      kmm_free(ptr);
    }
  else
    {
      __real_free(ptr);
    }
}

void *__wrap_realloc(void *ptr, size_t size)
{
  if (ptr != NULL && kmm_heapmember(ptr))
    {
      return kmm_realloc(ptr, size);
    }

  return __real_realloc(ptr, size);
}

size_t __wrap_malloc_size(void *ptr)
{
  if (ptr != NULL && kmm_heapmember(ptr))
    {
      return kmm_malloc_size(ptr);
    }

  return __real_malloc_size(ptr);
}
