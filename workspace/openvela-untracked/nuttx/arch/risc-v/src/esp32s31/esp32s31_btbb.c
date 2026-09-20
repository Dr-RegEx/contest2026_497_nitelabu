/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Awake-only NuttX adaptation of components/esp_phy/src/btbb_init.c.
 */

#include <nuttx/config.h>
#include <nuttx/mutex.h>
#include <assert.h>
#include <stdint.h>
#include "esp_private/btbb.h"

static mutex_t g_btbb_lock = NXMUTEX_INITIALIZER;
static unsigned int g_btbb_refs;

void esp_btbb_enable(void)
{
  nxmutex_lock(&g_btbb_lock);
  ASSERT(g_btbb_refs < UINT32_MAX);
  if (g_btbb_refs == 0)
    {
      bt_bb_v2_init_cmplx(1);
    }

  g_btbb_refs++;
  nxmutex_unlock(&g_btbb_lock);
}

void esp_btbb_disable(void)
{
  nxmutex_lock(&g_btbb_lock);
  ASSERT(g_btbb_refs > 0);
  g_btbb_refs--;
  nxmutex_unlock(&g_btbb_lock);
}
