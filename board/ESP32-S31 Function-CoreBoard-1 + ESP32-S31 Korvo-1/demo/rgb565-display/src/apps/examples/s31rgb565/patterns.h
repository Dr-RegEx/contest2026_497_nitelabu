/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DEMO_WIDTH 800
#define DEMO_HEIGHT 480
#define DEMO_PIXELS (DEMO_WIDTH * DEMO_HEIGHT)
#define DEMO_MODE_COUNT 7

static inline uint16_t rgb565(unsigned r, unsigned g, unsigned b)
{
    return (uint16_t)(((r & 0xf8u) << 8) | ((g & 0xfcu) << 3) | (b >> 3));
}

bool rgb565_selftest(void);
void rgb565_render(uint16_t *pixels, unsigned mode, unsigned frame);
const char *rgb565_mode_name(unsigned mode);
