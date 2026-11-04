// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <lvgl.h>
#ifdef __cplusplus
extern "C" {
#endif
// One generated UI instance per process; all calls run on the LVGL owner loop.
int coffee_window_create(lv_obj_t *parent);
void coffee_window_destroy(void);
bool coffee_window_owns(const lv_obj_t *parent);
lv_obj_t *coffee_widget_create(lv_obj_t *(*create)(lv_obj_t *), lv_obj_t *parent);
void coffee_window_load(lv_obj_t *panel, uint32_t delay_ms);
lv_anim_t *coffee_anim_start(lv_anim_t *animation);
#ifdef __cplusplus
}
#endif
