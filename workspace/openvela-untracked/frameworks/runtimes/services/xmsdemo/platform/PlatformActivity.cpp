// SPDX-License-Identifier: Apache-2.0
#include "PlatformActivity.h"
#include <BaseWindow.h>
#include <arpa/inet.h>
#include <cutils/properties.h>
#include <kvdb.h>
#include <malloc.h>
#include <netutils/netlib.h>
#include <sys/statfs.h>
#include <time.h>
#include <cstdio>
#include <cstring>
#ifdef CONFIG_XMSDEMO_PLATFORM_MEDIA
#include "MediaController.h"
#endif

namespace os::app::s31product {
static const char* kTheme = "persist.s31.demo.accent";

lv_obj_t* PlatformActivity::button(lv_obj_t* parent, const char* title,
                                  lv_event_cb_t callback)
{
    auto btn = lv_button_create(parent);
    lv_obj_set_size(btn, 175, 48);
    lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, this);
    auto label = lv_label_create(btn);
    lv_label_set_text(label, title);
    lv_obj_center(label);
    return label;
}

void PlatformActivity::onCreate()
{
    char value[PROPERTY_VALUE_MAX];
    property_get(kTheme, value, "blue");
    mTeal = !std::strcmp(value, "teal");
    mFontPath = vg_font_add_path("S31Product", "/etc/fonts/Montserrat-Regular.ttf");
    if (mFontPath) mFont = vg_font_create("S31Product", 20, LV_FREETYPE_FONT_STYLE_NORMAL);

    auto root = static_cast<lv_obj_t*>(getWindow()->getRoot());
    mPanel = lv_obj_create(root);
    lv_obj_set_size(mPanel, lv_pct(100), lv_pct(100));
    lv_obj_set_style_border_width(mPanel, 0, 0);
    lv_obj_set_style_radius(mPanel, 0, 0);
    lv_obj_set_style_pad_all(mPanel, 22, 0);
    lv_obj_set_style_pad_row(mPanel, 15, 0);
    lv_obj_set_style_text_color(mPanel, lv_color_hex(0xeff6ff), 0);
    if (mFont) lv_obj_set_style_text_font(mPanel, mFont, 0);
    lv_obj_set_flex_flow(mPanel, LV_FLEX_FLOW_COLUMN);

    auto title = lv_label_create(mPanel);
    lv_label_set_text(title, "Device / ESP32-S31");
    auto subtitle = lv_label_create(mPanel);
    lv_label_set_text(subtitle, "openvela on Korvo-1");
    mStatus = lv_label_create(mPanel);
    lv_obj_set_width(mStatus, lv_pct(100));
    lv_obj_set_style_text_line_space(mStatus, 12, 0);

#ifdef CONFIG_XMSDEMO_PLATFORM_MEDIA
    mAudioOwner = MediaController::instance().newOwner();
    mPendingAction = getIntent().mAction;
    mAudioStatus = lv_label_create(mPanel);
    auto audio = lv_obj_create(mPanel);
    lv_obj_set_size(audio, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(audio, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(audio, 0, 0);
    lv_obj_set_style_pad_all(audio, 0, 0);
    lv_obj_set_flex_flow(audio, LV_FLEX_FLOW_ROW_WRAP);
    button(audio, "Play tone", playClicked);
    button(audio, "Pause / resume", pauseClicked);
    button(audio, "Stop audio", stopClicked);
#endif

    auto actions = lv_obj_create(mPanel);
    lv_obj_set_size(actions, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_pad_all(actions, 0, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW_WRAP);
    mThemeLabel = button(actions, "Use teal", themeClicked);
    button(actions, "Widgets", widgetsClicked);
    button(actions, "Home", homeClicked);
    button(actions, "Close", exitClicked);
    mMessage = lv_label_create(mPanel);
    lv_label_set_text(mMessage, "Appearance is saved on this device.");
    applyTheme();
    refresh();
    std::printf("S31Product created theme=%s font=%s\n", mTeal ? "teal" : "blue",
          mFont && mFont != LV_FONT_DEFAULT ? "freetype" : "fallback");
}

void PlatformActivity::applyTheme()
{
    lv_obj_set_style_bg_color(mPanel, lv_color_hex(mTeal ? 0x123c3a : 0x172b4d), 0);
    lv_label_set_text(mThemeLabel, mTeal ? "Use blue" : "Use teal");
}

void PlatformActivity::refresh()
{
    if (!mStatus) return;
    struct statfs storage;
    struct in_addr address = {};
    struct timespec uptime = {};
    char storageText[64];
    char networkText[64] = "No address";
    if (statfs("/data", &storage) == 0)
        std::snprintf(storageText, sizeof(storageText), "%lu KiB free",
                      static_cast<unsigned long>(storage.f_bavail * storage.f_bsize / 1024));
    else std::snprintf(storageText, sizeof(storageText), "Unavailable");
    if (netlib_get_ipv4addr("wlan0", &address) == 0 && address.s_addr != 0 &&
        !inet_ntop(AF_INET, &address, networkText, sizeof(networkText)))
        std::snprintf(networkText, sizeof(networkText), "Unavailable");
    clock_gettime(CLOCK_MONOTONIC, &uptime);
    auto memory = mallinfo();
    char text[256];
    std::snprintf(text, sizeof(text),
                  "Storage: %s\nNetwork: %s\nMemory: %lu KiB free\nUptime: %lu min",
                  storageText, networkText, static_cast<unsigned long>(memory.fordblks / 1024),
                  static_cast<unsigned long>(uptime.tv_sec / 60));
    lv_label_set_text(mStatus, text);
#ifdef CONFIG_XMSDEMO_PLATFORM_MEDIA
    auto audio = MediaController::instance().snapshot(mAudioOwner);
    lv_label_set_text_fmt(mAudioStatus, "Audio: %s", audio.owned ?
                         MediaController::name(audio.state) : "Finishing previous session");
#endif
}

void PlatformActivity::refreshTimer(lv_timer_t* timer)
{
    static_cast<PlatformActivity*>(lv_timer_get_user_data(timer))->refresh();
}

void PlatformActivity::onResume()
{
#ifdef CONFIG_XMSDEMO_PLATFORM_MEDIA
    mResumed = true;
    std::string action;
    action.swap(mPendingAction);
    audioAction(action);
#endif
    refresh();
    if (!mTimer) mTimer = lv_timer_create(refreshTimer, 1000, this);
    std::printf("S31Product resumed\n");
}

void PlatformActivity::onPause()
{
#ifdef CONFIG_XMSDEMO_PLATFORM_MEDIA
    mResumed = false;
    MediaController::instance().stop(mAudioOwner);
#endif
    if (mTimer) { lv_timer_delete(mTimer); mTimer = nullptr; }
    std::printf("S31Product paused\n");
}

void PlatformActivity::onDestroy()
{
#ifdef CONFIG_XMSDEMO_PLATFORM_MEDIA
    mResumed = false;
    MediaController::instance().stop(mAudioOwner);
    mAudioStatus = nullptr;
#endif
    if (mTimer) { lv_timer_delete(mTimer); mTimer = nullptr; }
    // Delete styled objects before releasing their font. The window root
    // remains owned by WM and is removed after Activity::onDestroy returns.
    if (mPanel) { lv_obj_delete(mPanel); mPanel = nullptr; }
    mStatus = mMessage = mThemeLabel = nullptr;
    if (mFont) { vg_font_destroy(mFont); mFont = nullptr; }
    if (mFontPath) { vg_font_remove_path(mFontPath); mFontPath = nullptr; }
    std::printf("S31Product destroyed\n");
}

void PlatformActivity::themeClicked(lv_event_t* event)
{
    auto self = static_cast<PlatformActivity*>(lv_event_get_user_data(event));
    bool next = !self->mTeal;
    if (property_set(kTheme, next ? "teal" : "blue") != 0 || property_commit() != 0) {
        lv_label_set_text(self->mMessage, "Could not save. Please try again.");
        return;
    }
    self->mTeal = next;
    self->applyTheme();
    lv_label_set_text(self->mMessage, "Appearance saved.");
    std::printf("S31Product appearance saved=%s\n", next ? "teal" : "blue");
}

void PlatformActivity::widgetsClicked(lv_event_t* event)
{
    auto self = static_cast<PlatformActivity*>(lv_event_get_user_data(event));
    self->startActivity(Intent("com.vela.xmsdemo.lvgl/WidgetsActivity"));
}

void PlatformActivity::homeClicked(lv_event_t* event)
{
    auto self = static_cast<PlatformActivity*>(lv_event_get_user_data(event));
    Intent intent;
    intent.setAction(Intent::ACTION_HOME);
    self->startActivity(intent);
}

void PlatformActivity::exitClicked(lv_event_t* event)
{
    static_cast<PlatformActivity*>(lv_event_get_user_data(event))->finish();
}

#ifdef CONFIG_XMSDEMO_PLATFORM_MEDIA
void PlatformActivity::audioAction(const std::string& action)
{
    auto& audio = MediaController::instance();
    int ret = 0;
    if (action == "s31.audio.play") ret = audio.play(mAudioOwner);
    else if (action == "s31.audio.pause") ret = audio.pause(mAudioOwner);
    else if (action == "s31.audio.stop") audio.stop(mAudioOwner);
    else return;
    lv_label_set_text(mMessage, ret < 0 ? "Audio busy or unavailable. Please retry." :
                     "Test tone only. Leaving this page stops audio.");
    std::printf("S31Audio request=%s result=%d\n", action.c_str(), ret);
    refresh();
}

void PlatformActivity::onNewIntent(const Intent& intent)
{
    if (mResumed) audioAction(intent.mAction);
    else mPendingAction = intent.mAction;
}

void PlatformActivity::playClicked(lv_event_t* event)
{
    static_cast<PlatformActivity*>(lv_event_get_user_data(event))->audioAction("s31.audio.play");
}

void PlatformActivity::pauseClicked(lv_event_t* event)
{
    auto self = static_cast<PlatformActivity*>(lv_event_get_user_data(event));
    auto state = MediaController::instance().snapshot(self->mAudioOwner);
    self->audioAction(state.state == MediaController::State::Paused ?
                      "s31.audio.play" : "s31.audio.pause");
}

void PlatformActivity::stopClicked(lv_event_t* event)
{
    static_cast<PlatformActivity*>(lv_event_get_user_data(event))->audioAction("s31.audio.stop");
}
#endif
}
