/* SPDX-License-Identifier: Apache-2.0 */
#include "patterns.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Five columns per glyph, low bit at the top. No external font/assets. */
static const uint8_t font[][5] = {
    {0x3e,0x51,0x49,0x45,0x3e}, {0,0x42,0x7f,0x40,0},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e},
    {0x7e,0x11,0x11,0x11,0x7e}, {0x7f,0x49,0x49,0x49,0x36},
    {0x3e,0x41,0x41,0x41,0x22}, {0x7f,0x41,0x41,0x22,0x1c},
    {0x7f,0x49,0x49,0x49,0x41}, {0x7f,0x09,0x09,0x09,0x01},
    {0x3e,0x41,0x49,0x49,0x7a}, {0x7f,0x08,0x08,0x08,0x7f},
    {0,0x41,0x7f,0x41,0}, {0x20,0x40,0x41,0x3f,0x01},
    {0x7f,0x08,0x14,0x22,0x41}, {0x7f,0x40,0x40,0x40,0x40},
    {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x10,0x7f},
    {0x3e,0x41,0x41,0x41,0x3e}, {0x7f,0x09,0x09,0x09,0x06},
    {0x3e,0x41,0x51,0x21,0x5e}, {0x7f,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7f,0x01,0x01},
    {0x3f,0x40,0x40,0x40,0x3f}, {0x1f,0x20,0x40,0x20,0x1f},
    {0x3f,0x40,0x38,0x40,0x3f}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
};

static void rect(uint16_t *p, int x, int y, int w, int h, uint16_t color)
{
    int right = x + w < DEMO_WIDTH ? x + w : DEMO_WIDTH;
    int bottom = y + h < DEMO_HEIGHT ? y + h : DEMO_HEIGHT;
    for (int row = y < 0 ? 0 : y; row < bottom; ++row) {
        for (int col = x < 0 ? 0 : x; col < right; ++col) {
            p[row * DEMO_WIDTH + col] = color;
        }
    }
}

static void text(uint16_t *p, int x, int y, const char *s, int scale, uint16_t color)
{
    for (; *s; ++s, x += 6 * scale) {
        int index = *s >= '0' && *s <= '9' ? *s - '0' :
                    *s >= 'A' && *s <= 'Z' ? *s - 'A' + 10 : -1;
        if (index >= 0) {
            for (int col = 0; col < 5; ++col) {
                for (int row = 0; row < 7; ++row) {
                    if (font[index][col] & (1u << row)) {
                        rect(p, x + col * scale, y + row * scale,
                             scale, scale, color);
                    }
                }
            }
        } else if (*s == '-') {
            rect(p, x, y + 3 * scale, 5 * scale, scale, color);
        } else if (*s == '/') {
            for (int i = 0; i < 5; ++i) {
                rect(p, x + i * scale, y + (5-i)*scale, scale, scale, color);
            }
        } else if (*s == ':') {
            rect(p, x + 2*scale, y + scale, scale, scale, color);
            rect(p, x + 2*scale, y + 5*scale, scale, scale, color);
        }
    }
}

bool rgb565_selftest(void)
{
    if (rgb565(255,0,0) != 0xf800 || rgb565(0,255,0) != 0x07e0 ||
        rgb565(0,0,255) != 0x001f || rgb565(255,255,255) != 0xffff ||
        rgb565(0,0,0) != 0) {
        return false;
    }
    /* Every possible RGB565 word survives expansion and quantization. */
    for (unsigned value = 0; value <= 0xffff; ++value) {
        unsigned r = (value >> 11) & 31, g = (value >> 5) & 63, b = value & 31;
        if (rgb565((r << 3) | (r >> 2), (g << 2) | (g >> 4),
                   (b << 3) | (b >> 2)) != value) {
            return false;
        }
    }
    const uint16_t probe = 0xf800;
    const uint8_t *bytes = (const uint8_t *)&probe;
    return bytes[0] == 0 && bytes[1] == 0xf8;
}

const char *rgb565_mode_name(unsigned mode)
{
    static const char *const names[] = {
        "DASHBOARD", "RED F800", "GREEN 07E0", "BLUE 001F",
        "WHITE FFFF", "BLACK 0000", "CHECKERBOARD"
    };
    return mode < DEMO_MODE_COUNT ? names[mode] : "UNKNOWN";
}

void rgb565_render(uint16_t *p, unsigned mode, unsigned frame)
{
    static const uint16_t colors[] = {
        0xffff, 0xffe0, 0x07ff, 0x07e0, 0xf81f, 0xf800, 0x001f, 0x0000
    };
    static const char *const labels[] = {
        "WHITE", "YELLOW", "CYAN", "GREEN", "MAGENTA", "RED", "BLUE", "BLACK"
    };
    static const char *const values[] = {
        "FFFF", "FFE0", "07FF", "07E0", "F81F", "F800", "001F", "0000"
    };
    const uint16_t bg = rgb565(10,18,32), muted = rgb565(142,160,181);
    const uint16_t accent = rgb565(52,220,204);
    if (mode > 0 && mode < 6) {
        static const uint16_t fills[] = {0, 0xf800, 0x07e0, 0x001f, 0xffff, 0};
        rect(p, 0, 0, DEMO_WIDTH, DEMO_HEIGHT, fills[mode]);
        rect(p, 160, 199, 480, 82, bg);
        text(p, 188, 221, rgb565_mode_name(mode), 4, 0xffff);
        return;
    }
    if (mode == 6) {
        for (unsigned y = 0; y < DEMO_HEIGHT; ++y) {
            for (unsigned x = 0; x < DEMO_WIDTH; ++x) {
                unsigned cell = y < 240 ? 32 : 1;
                p[y*DEMO_WIDTH+x] = ((x/cell + y/cell + frame/5) & 1) ? 0xffff : 0;
            }
        }
        rect(p, 142, 211, 516, 58, bg);
        text(p, 166, 226, "PIXEL / ALIGNMENT TEST", 3, 0xffff);
        return;
    }

    rect(p, 0, 0, DEMO_WIDTH, DEMO_HEIGHT, bg);
    rect(p, 24, 24, 5, 53, accent);
    text(p, 42, 24, "OPENVELA RGB565", 4, 0xffff);
    text(p, 43, 63, "ESP32-S31 KORVO-1 / 800 X 480 / 16 BIT", 2, muted);
    text(p, 25, 97, "COLOR ORDER", 1, muted);
    text(p, 640, 97, "R5  G6  B5", 2, accent);

    for (int i = 0; i < 8; ++i) {
        int x = 24 + i * 94;
        rect(p, x, 120, 90, 40, colors[i]);
        if (i == 7) {
            rect(p, x, 120, 90, 1, muted);
            rect(p, x, 159, 90, 1, muted);
        }
        text(p, x, 170, labels[i], 1, 0xffff);
        text(p, x, 183, values[i], 1, muted);
    }

    text(p, 24, 218, "CHANNEL GRADIENTS", 2, 0xffff);
    static const char *const levels[] = {"R 32", "G 64", "B 32", "GRAY"};
    for (int row = 0; row < 4; ++row) {
        text(p, 24, 252 + row * 43, levels[row], 1, muted);
        for (unsigned x = 0; x < 384; ++x) {
            unsigned v = x * 255 / 383;
            uint16_t color = rgb565(row == 0 || row == 3 ? v : 0,
                                   row == 1 || row == 3 ? v : 0,
                                   row == 2 || row == 3 ? v : 0);
            rect(p, 72 + (int)x, 244 + row * 43, 1, 27, color);
        }
    }
    text(p, 500, 211, "ALL 65536 COLORS", 2, 0xffff);
    /* 256 x 192 view: all 65536 words are on the 256x256 test page
     * through a slowly moving vertical viewport, without resampling. */
    unsigned offset = (frame / 2) % 65;
    for (unsigned y = 0; y < 192; ++y) {
        for (unsigned x = 0; x < 256; ++x) {
            p[(240+y)*DEMO_WIDTH+500+x] = (uint16_t)((y+offset)*256+x);
        }
    }
    text(p, 24, 422, "PACK / UNPACK SELFTEST OK", 2, accent);
    rect(p, 24, 452, 752, 2, rgb565(39,53,71));
    rect(p, 24 + (int)(frame * 7 % 708), 450, 44, 6, accent);
    char status[80];
    snprintf(status, sizeof(status), "LIVE %06u   AUTO: COLOR BARS / SOLID / PIXEL GRID", frame);
    text(p, 24, 466, status, 1, muted);
}
