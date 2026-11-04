// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <nuttx/config.h>
#include <app/Activity.h>
#include <lvgl/lvgl.h>
#include <uikit/uikit_font_manager.h>
#include <cstdint>
#include <string>

namespace os::app::s31product {
class PlatformActivity : public Activity {
public:
    void onCreate() override;
    void onStart() override {}
    void onResume() override;
    void onPause() override;
    void onStop() override {}
    void onDestroy() override;
#ifdef CONFIG_XMSDEMO_PLATFORM_MEDIA
    void onNewIntent(const Intent& intent) override;
#endif
private:
    static void refreshTimer(lv_timer_t* timer);
    static void themeClicked(lv_event_t* event);
    static void widgetsClicked(lv_event_t* event);
    static void homeClicked(lv_event_t* event);
    static void exitClicked(lv_event_t* event);
    lv_obj_t* button(lv_obj_t* parent, const char* title, lv_event_cb_t callback);
    void refresh();
    void applyTheme();
    lv_obj_t* mPanel = nullptr;
    lv_obj_t* mStatus = nullptr;
    lv_obj_t* mMessage = nullptr;
    lv_obj_t* mThemeLabel = nullptr;
    lv_timer_t* mTimer = nullptr;
    lv_font_t* mFont = nullptr;
    vg_font_path_handle_t mFontPath = nullptr;
    bool mTeal = false;
#ifdef CONFIG_XMSDEMO_PLATFORM_MEDIA
    static void playClicked(lv_event_t* event);
    static void pauseClicked(lv_event_t* event);
    static void stopClicked(lv_event_t* event);
    void audioAction(const std::string& action);
    lv_obj_t* mAudioStatus = nullptr;
    uint64_t mAudioOwner = 0;
    std::string mPendingAction;
    bool mResumed = false;
#endif
};
}
