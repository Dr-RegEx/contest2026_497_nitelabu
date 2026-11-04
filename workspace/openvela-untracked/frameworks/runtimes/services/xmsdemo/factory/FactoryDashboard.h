// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <lvgl.h>

namespace os::app::factory {
enum class DashboardAction { AudioToggle, ProbeStorage, SelectLed, Coffee };
struct FactoryDashboard {
    using Callback = void (*)(void*, DashboardAction, int);
    Callback callback = nullptr;
    void* context = nullptr;
    lv_obj_t *camera_status_label = nullptr, *camera_canvas = nullptr;
    lv_draw_buf_t* camera_canvas_draw_buf = nullptr;
    lv_obj_t *audio_status_label = nullptr, *audio_btn_label = nullptr, *audio_progress = nullptr;
    lv_obj_t *storage_card = nullptr, *storage_status_label = nullptr;
    lv_obj_t *usb_hid_card = nullptr, *usb_hid_status_label = nullptr, *usb_hid_input_label = nullptr;
    lv_obj_t* button_led_buttons[4] = {};
    lv_obj_t *button_status_label = nullptr, *factory_screen = nullptr;
};
void factory_dashboard_create(FactoryDashboard* app, lv_obj_t* parent);
}
