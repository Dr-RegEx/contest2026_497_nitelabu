/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Native window adaptation of the pinned factory_demo/main/main.c dashboard.
 */
#include "FactoryDashboard.h"
#include <cstdint>
namespace os::app::factory {
using factory_demo_app_t = FactoryDashboard;
#define UI_REFRESH_PERIOD_MS     100
#define UI_DISPLAY_BUFFER_HEIGHT 160
#define UI_DISPLAY_TASK_STACK_SIZE (16 * 1024)
#define UI_SCREEN_SWITCH_TASK_STACK_SIZE 4096
#define UI_SCREEN_SWITCH_TASK_PRIORITY 5
#define UI_DISPLAY_ENABLE_PPA_ACCEL true
#define UI_CARD_RADIUS           22
#define UI_BORDER_WIDTH          1
#define UI_STATUS_TEXT_LEN       96
#define UI_LED_NAME_LEN          24
#define UI_CAMERA_FRAME_WIDTH    240
#define UI_CAMERA_FRAME_HEIGHT   240
#define UI_MAIN_ROW_Y            110
#define UI_MAIN_ROW_H            258
#define UI_BOTTOM_ROW_Y          382
#define UI_BOTTOM_ROW_H          92
#define UI_RIGHT_COL_X           524
#define UI_RIGHT_COL_W           252
#define UI_COMPACT_CARD_PAD      10
#define UI_COMPACT_CONTENT_Y     30
#define UI_COMPACT_BUTTON_Y      32
#define UI_COMPACT_STATUS_Y      66
#define UI_HW_BUTTON_COUNT       4
#define UI_AUDIO_BUTTON_Y        78
#define UI_AUDIO_BUTTON_W        104
#define UI_AUDIO_BUTTON_H        40
#define UI_AUDIO_BUTTON_GAP      8
#define UI_SDCARD_MOUNT_RETRY_DELAY_MS 100
#define UI_SDCARD_MOUNT_MAX_RETRY      2
#define UI_CAMERA_FIRST_FRAME_TIMEOUT_MS 3000

static const lv_color_t COLOR_BG = LV_COLOR_MAKE(0x07, 0x12, 0x22);
static const lv_color_t COLOR_PANEL = LV_COLOR_MAKE(0x10, 0x1F, 0x32);
static const lv_color_t COLOR_PANEL_CAMERA = LV_COLOR_MAKE(0x14, 0x26, 0x3D);
static const lv_color_t COLOR_PANEL_AUDIO = LV_COLOR_MAKE(0x12, 0x24, 0x36);
static const lv_color_t COLOR_PANEL_BUTTONS = LV_COLOR_MAKE(0x11, 0x21, 0x33);
static const lv_color_t COLOR_PANEL_USB = LV_COLOR_MAKE(0x12, 0x22, 0x34);
static const lv_color_t COLOR_PANEL_STORAGE = LV_COLOR_MAKE(0x11, 0x23, 0x35);
static const lv_color_t COLOR_TEXT = LV_COLOR_MAKE(0xF4, 0xF5, 0xF7);
static const lv_color_t COLOR_MUTED = LV_COLOR_MAKE(0x9E, 0xAE, 0xBF);
static const lv_color_t COLOR_ACCENT = LV_COLOR_MAKE(0xFF, 0x8A, 0x5B);
static const lv_color_t COLOR_SKY = LV_COLOR_MAKE(0x61, 0xA8, 0xFF);
static const lv_color_t COLOR_STATE_OK = LV_COLOR_MAKE(0x12, 0x4B, 0x38);
static const lv_color_t COLOR_STATE_WARN = LV_COLOR_MAKE(0x5A, 0x44, 0x12);
static const lv_color_t COLOR_STATE_ERR = LV_COLOR_MAKE(0x58, 0x20, 0x26);
static const lv_color_t COLOR_STATE_IDLE = LV_COLOR_MAKE(0x17, 0x42, 0x6A);

struct factory_button_led_map_t {
    const char* button_name;
    lv_color_t ui_color;
};
static const factory_button_led_map_t BUTTON_LED_MAP[4] = {
    {"set", LV_COLOR_MAKE(0xFF, 0xB0, 0x47)},
    {"mode", LV_COLOR_MAKE(0x61, 0xA8, 0xFF)},
    {"vol-", LV_COLOR_MAKE(0x63, 0xD5, 0xB4)},
    {"vol+", LV_COLOR_MAKE(0xFF, 0x6B, 0x5C)}
};
static void factory_style_card(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_radius(obj, UI_CARD_RADIUS, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_grad_color(obj, color, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, UI_BORDER_WIDTH, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x324B66), 0);
    lv_obj_set_style_shadow_width(obj, 18, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_black(), 0);
    lv_obj_set_style_pad_all(obj, 18, 0);
}

static void factory_style_compact_card(lv_obj_t *obj, lv_color_t color)
{
    factory_style_card(obj, color);
    lv_obj_set_style_shadow_width(obj, 14, 0);
    lv_obj_set_style_pad_all(obj, UI_COMPACT_CARD_PAD, 0);
}

static lv_obj_t *factory_create_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                                     const char *title, const char *subtitle, lv_color_t color)
{
    const lv_font_t *title_font = (h <= 96 && w < 320) ? &lv_font_montserrat_18 : &lv_font_montserrat_24;
    const lv_font_t *subtitle_font = &lv_font_montserrat_14;
    lv_coord_t subtitle_width = (h <= 96 && w > 260) ? (w - 230) : (w - 36);

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    factory_style_card(card, color);

    lv_obj_t *title_label = lv_label_create(card);
    lv_obj_set_style_text_font(title_label, title_font, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT, 0);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    if (subtitle) {
        lv_obj_t *subtitle_label = lv_label_create(card);
        lv_obj_set_width(subtitle_label, subtitle_width);
        lv_obj_set_style_text_font(subtitle_label, subtitle_font, 0);
        lv_obj_set_style_text_color(subtitle_label, COLOR_MUTED, 0);
        lv_label_set_text(subtitle_label, subtitle);
        lv_obj_align_to(subtitle_label, title_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
    }

    return card;
}

static lv_obj_t *factory_create_compact_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                                             const char *title, lv_color_t color)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    factory_style_compact_card(card, color);

    lv_obj_t *title_label = lv_label_create(card);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT, 0);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    return card;
}

static void on_audio_button(lv_event_t* event)
{
    auto app = static_cast<FactoryDashboard*>(lv_event_get_user_data(event));
    app->callback(app->context, DashboardAction::AudioToggle, 0);
}
static void on_sd_probe_button(lv_event_t* event)
{
    auto app = static_cast<FactoryDashboard*>(lv_event_get_user_data(event));
    app->callback(app->context, DashboardAction::ProbeStorage, 0);
}
static void on_coffee_demo_button(lv_event_t* event)
{
    auto app = static_cast<FactoryDashboard*>(lv_event_get_user_data(event));
    app->callback(app->context, DashboardAction::Coffee, 0);
}
static void on_button_led_clicked(lv_event_t* event)
{
    auto app = static_cast<FactoryDashboard*>(lv_event_get_user_data(event));
    for (int i = 0; i < 4; ++i)
        if (lv_event_get_target(event) == app->button_led_buttons[i])
            app->callback(app->context, DashboardAction::SelectLed, i);
}
static lv_obj_t *factory_create_action_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                                              const char *text, lv_event_cb_t cb, void *user_data, lv_color_t color)
{
    const lv_font_t *button_font = (w < 96) ? &lv_font_montserrat_14 : &lv_font_montserrat_18;

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 18, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, button_font, 0);
    lv_obj_set_style_text_color(label, COLOR_TEXT, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

static lv_obj_t *factory_create_button_led_key(FactoryDashboard *app, lv_obj_t *parent, int index, lv_coord_t x, lv_coord_t y)
{
    const factory_button_led_map_t *map = &BUTTON_LED_MAP[index];
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, 108, 34);
    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A2D43), static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, map->ui_color, static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(btn, map->ui_color, static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x344D67), static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn, lv_color_white(), static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, on_button_led_clicked, LV_EVENT_PRESSED, app);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, COLOR_TEXT, 0);
    lv_label_set_text(label, map->button_name);
    lv_obj_center(label);

    return btn;
}

void factory_dashboard_create(FactoryDashboard *app, lv_obj_t *parent)
{
    lv_obj_t *scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 800, 480);
    app->factory_screen = scr;
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *orb_a = lv_obj_create(scr);
    lv_obj_remove_style_all(orb_a);
    lv_obj_set_size(orb_a, 260, 260);
    lv_obj_set_pos(orb_a, -60, -40);
    lv_obj_set_style_radius(orb_a, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(orb_a, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(orb_a, LV_OPA_20, 0);

    lv_obj_t *orb_b = lv_obj_create(scr);
    lv_obj_remove_style_all(orb_b);
    lv_obj_set_size(orb_b, 220, 220);
    lv_obj_set_pos(orb_b, 620, 290);
    lv_obj_set_style_radius(orb_b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(orb_b, COLOR_SKY, 0);
    lv_obj_set_style_bg_opa(orb_b, LV_OPA_20, 0);

    lv_obj_t *header = factory_create_card(scr, 24, 20, 752, 72,
                                           "Korvo Factory Demo",
                                           "LCD / Camera / Audio / LED / SD / Buttons",
                                           COLOR_PANEL);
    lv_obj_t *header_chip = lv_btn_create(header);
    lv_obj_set_size(header_chip, 174, 34);
    lv_obj_align(header_chip, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_radius(header_chip, 17, 0);
    lv_obj_set_style_bg_color(header_chip, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(header_chip, LV_OPA_90, 0);
    lv_obj_set_style_border_width(header_chip, 0, 0);
    lv_obj_set_style_pad_all(header_chip, 0, 0);
    lv_obj_set_style_shadow_width(header_chip, 0, 0);
    lv_obj_add_event_cb(header_chip, on_coffee_demo_button, LV_EVENT_CLICKED, app);
    lv_obj_t *chip_label = lv_label_create(header_chip);
    lv_obj_set_style_text_color(chip_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(chip_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(chip_label, "Coffee Demo");
    lv_obj_center(chip_label);

    lv_obj_t *camera_card = lv_obj_create(scr);
    lv_obj_set_pos(camera_card, 24, UI_MAIN_ROW_Y);
    lv_obj_set_size(camera_card, 484, UI_MAIN_ROW_H);
    lv_obj_clear_flag(camera_card, LV_OBJ_FLAG_SCROLLABLE);
    factory_style_card(camera_card, COLOR_PANEL_CAMERA);
    lv_obj_set_style_shadow_width(camera_card, 10, 0);
    lv_obj_set_style_pad_all(camera_card, 8, 0);

    lv_obj_t *camera_frame = lv_obj_create(camera_card);
    lv_obj_set_pos(camera_frame, 0, 0);
    lv_obj_set_size(camera_frame, UI_CAMERA_FRAME_WIDTH, UI_CAMERA_FRAME_HEIGHT);
    lv_obj_clear_flag(camera_frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(camera_frame, 20, 0);
    lv_obj_set_style_bg_color(camera_frame, lv_color_hex(0x05080E), 0);
    lv_obj_set_style_bg_opa(camera_frame, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(camera_frame, lv_color_hex(0x2D405A), 0);
    lv_obj_set_style_border_width(camera_frame, 1, 0);
    lv_obj_set_style_pad_all(camera_frame, 0, 0);

    lv_obj_t *camera_frame_tag = lv_obj_create(camera_frame);
    lv_obj_set_size(camera_frame_tag, 88, 26);
    lv_obj_align(camera_frame_tag, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_set_style_radius(camera_frame_tag, 13, 0);
    lv_obj_set_style_bg_color(camera_frame_tag, lv_color_hex(0x1A2E48), 0);
    lv_obj_set_style_bg_opa(camera_frame_tag, LV_OPA_80, 0);
    lv_obj_set_style_border_width(camera_frame_tag, 0, 0);
    lv_obj_set_style_pad_all(camera_frame_tag, 0, 0);

    lv_obj_t *camera_frame_tag_label = lv_label_create(camera_frame_tag);
    lv_obj_set_style_text_font(camera_frame_tag_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(camera_frame_tag_label, COLOR_TEXT, 0);
    lv_label_set_text(camera_frame_tag_label, "240 preview");
    lv_obj_center(camera_frame_tag_label);

    app->camera_canvas = lv_canvas_create(camera_frame);
    if (app->camera_canvas_draw_buf) {
        lv_canvas_set_draw_buf(app->camera_canvas, app->camera_canvas_draw_buf);
        lv_canvas_fill_bg(app->camera_canvas, lv_color_black(), LV_OPA_COVER);
    }
    lv_obj_center(app->camera_canvas);
    lv_obj_move_foreground(camera_frame_tag);

    lv_obj_t *camera_title = lv_label_create(camera_card);
    lv_obj_set_style_text_font(camera_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(camera_title, COLOR_TEXT, 0);
    lv_label_set_text(camera_title, "Camera");
    lv_obj_set_pos(camera_title, 258, 4);

    lv_obj_t *camera_note = lv_label_create(camera_card);
    lv_obj_set_width(camera_note, 190);
    lv_obj_set_style_text_font(camera_note, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(camera_note, COLOR_MUTED, 0);
    lv_label_set_long_mode(camera_note, LV_LABEL_LONG_WRAP);
    lv_label_set_text(camera_note, "Auto camera, 240 x 240 preview");
    lv_obj_set_pos(camera_note, 258, 34);

    app->camera_status_label = lv_label_create(camera_card);
    lv_obj_set_width(app->camera_status_label, 190);
    lv_obj_set_style_text_font(app->camera_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(app->camera_status_label, COLOR_TEXT, 0);
    lv_label_set_long_mode(app->camera_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(app->camera_status_label, 258, 72);
    lv_label_set_text(app->camera_status_label, "");

    lv_obj_t *audio_card = factory_create_card(scr, UI_RIGHT_COL_X, UI_MAIN_ROW_Y, UI_RIGHT_COL_W, 160,
                                               "Audio Test",
                                               NULL,
                                               COLOR_PANEL_AUDIO);
    lv_obj_set_style_shadow_width(audio_card, 10, 0);
    app->audio_progress = lv_bar_create(audio_card);
    lv_obj_set_size(app->audio_progress, 216, 14);
    lv_obj_set_pos(app->audio_progress, 0, 32);
    lv_bar_set_range(app->audio_progress, 0, 100);
    lv_bar_set_value(app->audio_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(app->audio_progress, lv_color_hex(0x1E3248), static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(app->audio_progress, LV_OPA_COVER, static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(app->audio_progress, COLOR_ACCENT, static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(app->audio_progress, 7, static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(app->audio_progress, 7, static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_DEFAULT);

    lv_obj_t *audio_btn = factory_create_action_button(audio_card, 0, 54, 216, UI_AUDIO_BUTTON_H,
                                                       "Record", on_audio_button, app, COLOR_ACCENT);
    app->audio_btn_label = lv_obj_get_child(audio_btn, 0);

    app->audio_status_label = lv_label_create(audio_card);
    lv_obj_set_style_text_font(app->audio_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(app->audio_status_label, COLOR_MUTED, 0);
    lv_label_set_text(app->audio_status_label, "Unavailable");
    lv_obj_set_pos(app->audio_status_label, 0, 100);

    app->storage_card = factory_create_card(scr, UI_RIGHT_COL_X, 286, UI_RIGHT_COL_W, 82,
                                            "Storage",
                                            NULL,
                                            COLOR_PANEL_STORAGE);
    lv_obj_set_style_shadow_width(app->storage_card, 10, 0);
    app->storage_status_label = lv_label_create(app->storage_card);
    lv_obj_set_width(app->storage_status_label, 136);
    lv_obj_set_height(app->storage_status_label, 24);
    lv_obj_set_style_text_font(app->storage_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(app->storage_status_label, COLOR_TEXT, 0);
    lv_label_set_long_mode(app->storage_status_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(app->storage_status_label, "Not checked");
    lv_obj_set_pos(app->storage_status_label, 0, 34);
    factory_create_action_button(app->storage_card, 148, 22, 78, 34, "Probe", on_sd_probe_button, app, COLOR_SKY);

    lv_obj_t *button_led_card = factory_create_compact_card(scr, 24, UI_BOTTOM_ROW_Y, 486, UI_BOTTOM_ROW_H,
                                                            "Buttons / LED",
                                                            COLOR_PANEL_BUTTONS);
    lv_obj_set_style_shadow_width(button_led_card, 10, 0);
    app->button_led_buttons[0] = factory_create_button_led_key(app, button_led_card, 0, 0, 24);
    app->button_led_buttons[1] = factory_create_button_led_key(app, button_led_card, 1, 116, 24);
    app->button_led_buttons[2] = factory_create_button_led_key(app, button_led_card, 2, 232, 24);
    app->button_led_buttons[3] = factory_create_button_led_key(app, button_led_card, 3, 348, 24);

    app->button_status_label = lv_label_create(button_led_card);
    lv_obj_set_width(app->button_status_label, 456);
    lv_obj_set_height(app->button_status_label, 20);
    lv_obj_set_style_text_font(app->button_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(app->button_status_label, COLOR_MUTED, 0);
    lv_label_set_long_mode(app->button_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(app->button_status_label, 0, 60);

    app->usb_hid_card = factory_create_compact_card(scr, 526, UI_BOTTOM_ROW_Y, 250, UI_BOTTOM_ROW_H,
                                                    "USB HID",
                                                    COLOR_PANEL_USB);
    lv_obj_set_style_shadow_width(app->usb_hid_card, 10, 0);
    app->usb_hid_status_label = lv_label_create(app->usb_hid_card);
    lv_obj_set_width(app->usb_hid_status_label, 226);
    lv_obj_set_height(app->usb_hid_status_label, 22);
    lv_obj_set_style_text_font(app->usb_hid_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(app->usb_hid_status_label, COLOR_TEXT, 0);
    lv_label_set_long_mode(app->usb_hid_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(app->usb_hid_status_label, 0, 26);

    app->usb_hid_input_label = lv_label_create(app->usb_hid_card);
    lv_obj_set_width(app->usb_hid_input_label, 226);
    lv_obj_set_height(app->usb_hid_input_label, 22);
    lv_obj_set_style_text_font(app->usb_hid_input_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(app->usb_hid_input_label, COLOR_MUTED, 0);
    lv_label_set_long_mode(app->usb_hid_input_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(app->usb_hid_input_label, 0, 52);


}

}
