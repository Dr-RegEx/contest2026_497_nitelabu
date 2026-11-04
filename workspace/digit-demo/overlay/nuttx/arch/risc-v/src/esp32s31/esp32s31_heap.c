/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_heap.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>
#include <debug.h>
#include <nuttx/kmalloc.h>

#include "esp_heap_caps.h"
#include "esp_heap_caps_init.h"
#include "esp_psram.h"
#include "esp_private/esp_psram_extram.h"
#include "heap_memory_layout.h"
#include "soc/soc.h"
#include "esp32s31_heap.h"

/* The locked HAL's NuttX heap-registration entry point is a no-op.  Let its
 * PSRAM mapper provide the actual free interval (excluding external BSS and
 * other reserved sections), then register that interval with NuttX.  Wrap
 * only in FLAT+SPIRAM_USER_HEAP builds; the kernel/MMU page pool is separate.
 */

static bool g_registering;
static bool g_registered;

esp_err_t __real_heap_caps_add_region_with_caps(const uint32_t caps[],
                                               intptr_t start, intptr_t end);
esp_err_t __wrap_heap_caps_add_region_with_caps(const uint32_t caps[],
                                               intptr_t start, intptr_t end);

esp_err_t __wrap_heap_caps_add_region_with_caps(const uint32_t caps[],
                                               intptr_t start, intptr_t end)
{
  uint32_t allcaps = 0;
  int i;

  if (!g_registering)
    {
      return __real_heap_caps_add_region_with_caps(caps, start, end);
    }

  if (caps == NULL || start < SOC_EXTRAM_LOW || end > SOC_EXTRAM_HIGH ||
      end <= start)
    {
      return ESP_ERR_INVALID_ARG;
    }

  for (i = 0; i < SOC_MEMORY_TYPE_NO_PRIOS; i++)
    {
      allcaps |= caps[i];
    }

  if ((allcaps & (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) !=
      (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))
    {
      return ESP_ERR_NOT_SUPPORTED;
    }

  /* S31 has one byte-addressable PSRAM region.  Do not register it twice
   * or consume more than the single additional region this port supports.
   */

  if (g_registered)
    {
      return ESP_ERR_INVALID_STATE;
    }

  kumm_addregion((void *)start, end - start);
  g_registered = true;
  return ESP_OK;
}

void esp32s31_addregion(void)
{
  esp_err_t ret;

  if (!esp_psram_is_initialized())
    {
      merr("ERROR: PSRAM not initialized; external heap unavailable\n");
      return;
    }

  g_registering = true;
  ret = esp_psram_extram_add_to_heap_allocator();
  g_registering = false;
  if (ret != ESP_OK || !g_registered)
    {
      merr("ERROR: PSRAM heap registration failed: %d\n", ret);
      PANIC();
    }
}
