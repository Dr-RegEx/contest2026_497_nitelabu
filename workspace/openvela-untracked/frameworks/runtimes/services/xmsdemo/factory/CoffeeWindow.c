// SPDX-License-Identifier: Apache-2.0
#include "CoffeeWindow.h"
#include "coffee_assets.h"
#include "coffee/ui/ui.h"
#include <errno.h>

static lv_obj_t *owner;
static lv_obj_t *active_panel;
static lv_obj_t *pending_panel;
static lv_timer_t *navigation;
static lv_theme_t *simple_theme;
static bool closing;

lv_obj_t *coffee_widget_create(lv_obj_t *(*create)(lv_obj_t *), lv_obj_t *parent)
{
    bool screen = parent == NULL;
    lv_obj_t *obj = create(screen ? owner : parent);
    // The generated project assumes the simple theme. Apply it only to its own
    // widgets, before generated local styles; never change the display theme.
    lv_obj_remove_style_all(obj);
    if (screen) {
        lv_obj_set_size(obj, 800, 480);
        lv_obj_set_pos(obj, 0, 0);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(obj, LV_FONT_DEFAULT, 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(obj, lv_palette_lighten(LV_PALETTE_GREY, 4), 0);
        lv_obj_set_style_text_color(obj, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    } else if (simple_theme && simple_theme->apply_cb) {
        simple_theme->apply_cb(simple_theme, obj);
    }
    return obj;
}

lv_anim_t *coffee_anim_start(lv_anim_t *animation)
{
    // Generated animations bind var to their target. LVGL object deletion and
    // our subtree cancellation can now identify them, including infinite loops.
    lv_anim_t *started = lv_anim_start(animation);
    if (!started && animation->deleted_cb) animation->deleted_cb(animation);
    return started;
}

static void cancel_subtree(lv_obj_t *obj)
{
    lv_anim_delete(obj, NULL);
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        cancel_subtree(lv_obj_get_child(obj, (int32_t)i));
}

static void complete_navigation(lv_timer_t *timer)
{
    (void)timer;
    lv_obj_t *next = pending_panel;
    lv_obj_t *previous = active_panel;
    pending_panel = NULL;
    if (navigation) { lv_timer_delete(navigation); navigation = NULL; }
    if (!owner || closing || !next) return;
    // Clear the pending request before dispatch: LOADED may schedule another
    // transition (Brewing -> Done -> Home) and must keep that new timer alive.
    active_panel = next;
    lv_obj_send_event(next, LV_EVENT_SCREEN_LOAD_START, NULL);
    if (previous) lv_obj_add_flag(previous, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(next, LV_OBJ_FLAG_HIDDEN);
    lv_obj_send_event(next, LV_EVENT_SCREEN_LOADED, NULL);
    if (previous) {
        lv_obj_send_event(previous, LV_EVENT_SCREEN_UNLOADED, NULL);
        cancel_subtree(previous);
    }
}

void coffee_window_load(lv_obj_t *panel, uint32_t delay_ms)
{
    if (!owner || closing || !panel || panel == active_panel || panel == pending_panel) return;
    if (lv_obj_get_parent(panel) != owner) return;
    if (navigation) { lv_timer_delete(navigation); navigation = NULL; }
    pending_panel = panel;
    if (active_panel) lv_obj_send_event(active_panel, LV_EVENT_SCREEN_UNLOAD_START, NULL);
    // Upstream helper intentionally ignores fade mode/duration. Retain its
    // delay and screen event ordering while keeping the WM active screen intact.
    if (!delay_ms) complete_navigation(NULL);
    else {
        navigation = lv_timer_create(complete_navigation, delay_ms, NULL);
        if (!navigation) complete_navigation(NULL);
    }
}

int coffee_window_create(lv_obj_t *parent)
{
    if (!parent) return -EINVAL;
    if (owner) return -EBUSY;
    int ret = coffee_assets_init();
    if (ret) return ret < 0 ? ret : -EIO;
    for (int i = 0; i < COFFEE_ASSET_COUNT; ++i)
        if (!coffee_assets_get_src((coffee_asset_id_t)i)) return -ENOENT;
    simple_theme = lv_theme_simple_get();
    if (!simple_theme) simple_theme = lv_theme_simple_init(lv_obj_get_display(parent));
    if (!simple_theme) return -ENOMEM;
    owner = parent;
    closing = false;
    ui_init();
    return 0;
}

bool coffee_window_owns(const lv_obj_t *parent) { return parent && owner == parent; }

void coffee_window_destroy(void)
{
    if (!owner) return;
    closing = true;
    if (navigation) { lv_timer_delete(navigation); navigation = NULL; }
    pending_panel = active_panel = NULL;
    cancel_subtree(owner);
    ui_destroy();
    if (ui_Startevents____initial_actions0) {
        lv_obj_delete(ui_Startevents____initial_actions0);
        ui_Startevents____initial_actions0 = NULL;
    }
    // Delete events clear the generated theme registry's object references.
    // Only now may its callback user-data nodes be released.
    for (uint32_t i = 0; i < _ui_local_style_count; ++i) {
        _ui_local_style_property_setting_t *node = _ui_local_styles[i].style_property_settings;
        while (node) {
            _ui_local_style_property_setting_t *next = node->next_p;
            lv_free(node);
            node = next;
        }
    }
    lv_free(_ui_local_styles);
    _ui_local_styles = NULL;
    _ui_local_style_count = 0;
    owner = NULL;
    simple_theme = NULL; // Process-wide simple-theme styles remain LVGL owned.
    closing = false;
}
