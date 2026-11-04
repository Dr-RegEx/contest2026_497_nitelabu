/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COFFEE_ASSET_UI_IMG_BG_PNG,
    COFFEE_ASSET_UI_IMG_SHAPE1_PNG,
    COFFEE_ASSET_UI_IMG_PLAY_BUTTON_PNG,
    COFFEE_ASSET_UI_IMG_BIG_ESPRESSO_PNG,
    COFFEE_ASSET_UI_IMG_BIG_RISTRETTO_PNG,
    COFFEE_ASSET_UI_IMG_BIG_LATTE_PNG,
    COFFEE_ASSET_UI_IMG_ARROW_PNG,
    COFFEE_ASSET_UI_IMG_BLE_PNG,
    COFFEE_ASSET_UI_IMG_WIFI_PNG,
    COFFEE_ASSET_UI_IMG_SETTINGS_PNG,
    COFFEE_ASSET_UI_IMG_LATTE_ILLUSTRATION_PNG,
    COFFEE_ASSET_UI_IMG_CHEVRON_PNG,
    COFFEE_ASSET_UI_IMG_266850372,
    COFFEE_ASSET_UI_IMG_DISABLEDBEAN_PNG,
    COFFEE_ASSET_UI_IMG_BEANS_WHITE_PNG,
    COFFEE_ASSET_UI_IMG_CUP_PNG,
    COFFEE_ASSET_UI_IMG_CUPS_PNG,
    COFFEE_ASSET_UI_IMG_COFFEE_ILLUSTRATION_PNG,
    COFFEE_ASSET_UI_IMG_ESPRESSO_PNG,
    COFFEE_ASSET_UI_IMG_SLIDER_BG_PNG,
    COFFEE_ASSET_UI_IMG_TOP_PNG,
    COFFEE_ASSET_UI_IMG_PARAMETERS_PNG,
    COFFEE_ASSET_UI_IMG_1726788397,
    COFFEE_ASSET_UI_IMG_TEMP_BODY_PNG,
    COFFEE_ASSET_UI_IMG_TEMPERATURE_PNG,
    COFFEE_ASSET_UI_IMG_FOAM_BODY_PNG,
    COFFEE_ASSET_UI_IMG_FOAM_PNG,
    COFFEE_ASSET_UI_IMG_DONE_PNG,
    COFFEE_ASSET_UI_IMG_STANDBY_PNG,
    COFFEE_ASSET_UI_IMG_LIGHT_PNG,
    COFFEE_ASSET_UI_IMG_WATER_PNG,
    COFFEE_ASSET_UI_IMG_CLEAN_PNG,
    COFFEE_ASSET_UI_IMG_TASTE1_PNG,
    COFFEE_ASSET_UI_IMG_CUP1_PNG,
    COFFEE_ASSET_UI_IMG_UNITS_PNG,
    COFFEE_ASSET_UI_IMG_LANGUAGES_PNG,
    COFFEE_ASSET_UI_IMG_PROFILE_PNG,
    COFFEE_ASSET_UI_IMG_ROLLER_PNG,
    COFFEE_ASSET_UI_IMG_ALERT_PNG,
    COFFEE_ASSET_UI_IMG_TANK_PNG,
    COFFEE_ASSET_UI_IMG_WAVE1_PNG,
    COFFEE_ASSET_UI_IMG_WAVE_BOTTOM_PNG,
    COFFEE_ASSET_UI_IMG_DONE_60_PNG,
    COFFEE_ASSET_UI_IMG_ARROWS_PNG,
    COFFEE_ASSET_UI_IMG_BIGBEAN_PNG,
    COFFEE_ASSET_COUNT
} coffee_asset_id_t;

/* Modified for openvela: native status codes, resource validation and lifecycle.
 * All calls run on the UI thread/under its LVGL lock. Init follows lv_init().
 * Default directory is a target mount, never a build-host path.
 */
#ifndef COFFEE_ASSETS_DEFAULT_DIRECTORY
#define COFFEE_ASSETS_DEFAULT_DIRECTORY "/mnt/sdcard/factory-demo/coffee"
#endif
#define COFFEE_ASSETS_CACHE_LIMIT (2u * 1024u * 1024u)

/* Return 0 or negative errno. Re-init at the same directory is idempotent;
 * another directory requires deleting UI objects and calling deinit first.
 */
int coffee_assets_init(void);
int coffee_assets_init_at(const char *directory);

/* Delete UI objects and finish rendering before deinit. Shared LVGL services
 * remain registered; only this package's image/header cache entries are dropped.
 */
void coffee_assets_deinit(void);

/* Last failed asset, or COFFEE_ASSET_COUNT for configuration/lifecycle errors. */
coffee_asset_id_t coffee_assets_failed_id(void);
const char *coffee_assets_get_name(coffee_asset_id_t id);
const char *coffee_assets_get_symbol(coffee_asset_id_t id);
/* NULL before successful init, after deinit, or for an invalid ID. */
const char *coffee_assets_get_src(coffee_asset_id_t id);

#ifdef __cplusplus
}
#endif
