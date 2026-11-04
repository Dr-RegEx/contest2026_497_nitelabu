/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modified for openvela: retain upstream mapping; use LVGL's POSIX/libpng
 * services with bounded cache policy, integrity checks and scoped cleanup.
 */
#include "coffee_assets.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "lvgl.h"

#ifdef CONFIG_ESP32S31_VISION
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

/* Decoded backgrounds exceed 1 MiB.  The kernel-build process heap keeps
 * its high-water allocation after free(); use reclaimable shared pages for
 * these temporary pixels so the original vision service can start again.
 * Keep the source PNGs and the native libpng/LVGL decoder unchanged.
 */
struct coffee_pixel_mapping {
    size_t size;
    int fd;
};

#define COFFEE_PIXEL_PREFIX 64u
static lv_draw_buf_malloc_cb s_pixel_malloc;
static lv_draw_buf_free_cb s_pixel_free;
static bool s_pixel_mapping;

static void *pixel_malloc(size_t size, lv_color_format_t format)
{
    (void)format;
    if (size > SIZE_MAX - COFFEE_PIXEL_PREFIX) return NULL;
    size_t length = size + COFFEE_PIXEL_PREFIX;
    int fd = memfd_create("coffee-pixels", MFD_CLOEXEC);
    if (fd < 0) return NULL;
    if (ftruncate(fd, length) < 0) { close(fd); return NULL; }
    void *base = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) { close(fd); return NULL; }
    struct coffee_pixel_mapping *mapping = base;
    mapping->size = length;
    mapping->fd = fd;
    return (uint8_t *)base + COFFEE_PIXEL_PREFIX;
}

static void pixel_free(void *pixels)
{
    if (!pixels) return;
    struct coffee_pixel_mapping *mapping =
        (void *)((uint8_t *)pixels - COFFEE_PIXEL_PREFIX);
    size_t length = mapping->size;
    int fd = mapping->fd;
    munmap(mapping, length);
    close(fd);
}

static void pixel_mapping_enable(void)
{
    lv_draw_buf_handlers_t *handlers = &LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers;
    s_pixel_malloc = handlers->buf_malloc_cb;
    s_pixel_free = handlers->buf_free_cb;
    handlers->buf_malloc_cb = pixel_malloc;
    handlers->buf_free_cb = pixel_free;
    s_pixel_mapping = true;
}

static void pixel_mapping_disable(void)
{
    if (!s_pixel_mapping) return;
    lv_draw_buf_handlers_t *handlers = &LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers;
    handlers->buf_malloc_cb = s_pixel_malloc;
    handlers->buf_free_cb = s_pixel_free;
    s_pixel_mapping = false;
}
#endif

#if !LV_USE_LIBPNG || !LV_USE_FS_POSIX
#error "Coffee assets require LV_USE_LIBPNG and LV_USE_FS_POSIX"
#endif
#if LV_FS_POSIX_LETTER < 'A' || LV_FS_POSIX_LETTER > 'Z'
#error "Coffee assets require an uppercase LV_FS_POSIX_LETTER"
#endif
#if !LV_USE_DRAW_SW || LV_DRAW_SW_DRAW_UNIT_CNT != 1
#error "Re-evaluate the coffee image memory budget for a different renderer"
#endif

#define COFFEE_PATH_CAPACITY 256u
#define COFFEE_DIRECTORY_CAPACITY 192u

struct coffee_asset_entry {
    const char *symbol;
    const char *name;
    uint32_t encoded_size;
    uint32_t crc;
    uint16_t width;
    uint16_t height;
};

#define COFFEE_ASSET_ROW(id, symbol, name, size, crc, width, height) \
    [id] = {symbol, name, size, crc, width, height},
static const struct coffee_asset_entry s_assets[COFFEE_ASSET_COUNT] = {
#include "coffee_assets_table.inc"
};
#undef COFFEE_ASSET_ROW

/* Paths outlive UI objects and LVGL cache references. One application instance
 * owns this table. Nothing is registered in LVGL by this module.
 */
static char s_paths[COFFEE_ASSET_COUNT][COFFEE_PATH_CAPACITY];
static char s_directory[COFFEE_DIRECTORY_CAPACITY];
static bool s_initialized;
static coffee_asset_id_t s_failed_id = COFFEE_ASSET_COUNT;

static bool valid_id(coffee_asset_id_t id)
{
    return (unsigned int)id < COFFEE_ASSET_COUNT;
}

static int fs_error(lv_fs_res_t result)
{
    switch (result) {
    case LV_FS_RES_OK: return 0;
    case LV_FS_RES_NOT_EX: return -ENOENT;
    case LV_FS_RES_OUT_OF_MEM: return -ENOMEM;
    case LV_FS_RES_DENIED: return -EACCES;
    case LV_FS_RES_BUSY:
    case LV_FS_RES_LOCKED: return -EBUSY;
    case LV_FS_RES_TOUT: return -ETIMEDOUT;
    case LV_FS_RES_NOT_IMP: return -ENOSYS;
    case LV_FS_RES_INV_PARAM: return -EINVAL;
    default: return -EIO;
    }
}

static void release_paths(void)
{
    for (unsigned int i = 0; i < COFFEE_ASSET_COUNT; ++i) {
        if (s_paths[i][0] != '\0') {
            /* This also drops the corresponding header entry. Never pass NULL:
             * that would invalidate images belonging to other applications.
             */
            if (lv_is_initialized()) {
                lv_image_cache_drop(s_paths[i]);
            }
            s_paths[i][0] = '\0';
        }
    }
    s_directory[0] = '\0';
    s_initialized = false;
#ifdef CONFIG_ESP32S31_VISION
    pixel_mapping_disable();
#endif
}

static int validate_file(unsigned int id)
{
    const struct coffee_asset_entry *entry = &s_assets[id];
    lv_fs_file_t file;
    uint8_t buffer[1024];
    uint32_t total = 0;
    uLong crc = crc32(0L, Z_NULL, 0);
    int ret = fs_error(lv_fs_open(&file, s_paths[id], LV_FS_MODE_RD));
    if (ret < 0) {
        return ret;
    }

    for (;;) {
        uint32_t count = 0;
        ret = fs_error(lv_fs_read(&file, buffer, sizeof(buffer), &count));
        if (ret < 0 || count == 0) {
            break;
        }
        if (count > entry->encoded_size - total) {
            ret = -EBADMSG;
            break;
        }
        total += count;
        crc = crc32(crc, buffer, count);
    }
    int close_ret = fs_error(lv_fs_close(&file));
    if (ret < 0) {
        return ret;
    }
    if (close_ret < 0) {
        return close_ret;
    }
    if (total != entry->encoded_size || (uint32_t)crc != entry->crc) {
        return -EBADMSG;
    }

    /* Ask the actual registered decoder for dimensions; no pixel allocation.
     * A missing decoder must fail initialization, not produce an empty screen.
     */
    lv_image_header_t header;
    lv_image_cache_drop(s_paths[id]);
    if (lv_image_decoder_get_info(s_paths[id], &header) != LV_RESULT_OK ||
        header.w != entry->width || header.h != entry->height ||
        header.cf != LV_COLOR_FORMAT_ARGB8888) {
        return -ENOTSUP;
    }
    return 0;
}

int coffee_assets_init(void)
{
    return coffee_assets_init_at(COFFEE_ASSETS_DEFAULT_DIRECTORY);
}

int coffee_assets_init_at(const char *directory)
{
    s_failed_id = COFFEE_ASSET_COUNT;
    if (directory == NULL || directory[0] != '/') {
        return -EINVAL;
    }
    size_t length = strlen(directory);
    if (length < 2 || directory[length - 1] == '/' ||
        strstr(directory, "/../") || strstr(directory, "/./") ||
        strcmp(directory + length - 2, "/.") == 0 ||
        (length >= 3 && strcmp(directory + length - 3, "/..") == 0)) {
        return -EINVAL;
    }
    if (length >= sizeof(s_directory)) {
        return -ENAMETOOLONG;
    }
    if (s_initialized) {
        return strcmp(s_directory, directory) == 0 ? 0 : -EBUSY;
    }
    if (!lv_is_initialized()) {
        return -EPERM;
    }
    /* Native absolute paths must not be silently redirected by a build prefix.
     * lv_init() owns POSIX driver/libpng registration and lv_deinit() teardown.
     */
    if (LV_FS_POSIX_PATH[0] != '\0' ||
        lv_fs_get_drv(LV_FS_POSIX_LETTER) == NULL ||
        !lv_fs_is_ready(LV_FS_POSIX_LETTER)) {
        return -ENODEV;
    }
    if (lv_image_decoder_get_size_expand() != 0) {
        return -ENOTSUP;
    }
    uint32_t cache_size = lv_image_cache_get_size();
    if (cache_size > COFFEE_ASSETS_CACHE_LIMIT) {
        return -E2BIG;
    }
    for (unsigned int i = 0; i < COFFEE_ASSET_COUNT; ++i) {
        uint32_t decoded_size = lv_draw_buf_width_to_stride(s_assets[i].width,
                                     LV_COLOR_FORMAT_ARGB8888) * s_assets[i].height;
        /* Stock libpng returns INVALID if a nonzero cache cannot hold an image.
         * Zero cache is supported: each image is freed on decoder close.
         */
        if (cache_size != 0 && decoded_size > cache_size) {
            return -ENOBUFS;
        }
        if (decoded_size > 1536000u) {
            return -E2BIG;
        }
    }
    memcpy(s_directory, directory, length + 1);
    for (unsigned int i = 0; i < COFFEE_ASSET_COUNT; ++i) {
        int size = snprintf(s_paths[i], sizeof(s_paths[i]), "%c:%s/%s",
                            LV_FS_POSIX_LETTER, directory, s_assets[i].name);
        if (size < 0 || (size_t)size >= sizeof(s_paths[i])) {
            release_paths();
            return -ENAMETOOLONG;
        }
        int ret = validate_file(i);
        if (ret < 0) {
            s_failed_id = (coffee_asset_id_t)i;
            release_paths();
            return ret;
        }
    }
#ifdef CONFIG_ESP32S31_VISION
    pixel_mapping_enable();
#endif
    s_initialized = true;
    return 0;
}

void coffee_assets_deinit(void)
{
    release_paths();
    s_failed_id = COFFEE_ASSET_COUNT;
}

coffee_asset_id_t coffee_assets_failed_id(void)
{
    return s_failed_id;
}

const char *coffee_assets_get_name(coffee_asset_id_t id)
{
    return valid_id(id) ? s_assets[id].name : NULL;
}

const char *coffee_assets_get_symbol(coffee_asset_id_t id)
{
    return valid_id(id) ? s_assets[id].symbol : NULL;
}

const char *coffee_assets_get_src(coffee_asset_id_t id)
{
    return s_initialized && valid_id(id) ? s_paths[id] : NULL;
}
