// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <app/Activity.h>
#include <lvgl.h>
#include <string>
#include "FactoryLifecycle.h"
#include "FactoryDashboard.h"

namespace os::app::factory {
class FactoryActivity : public Activity {
public:
    void onCreate() override;
    void onStart() override {}
    void onResume() override;
    void onPause() override;
    void onStop() override {}
    void onDestroy() override;
    void onNewIntent(const Intent& intent) override;
private:
    static void tick(lv_timer_t* timer);
    static void action(void* context, DashboardAction action, int argument);
    static void factoryClicked(lv_event_t* event);
    static void digitClicked(lv_event_t* event);
    static void returnClicked(lv_event_t* event);
    void enterDigit(bool enter);
    static void displayEvent(lv_event_t* event);
    void refresh();
    void pollBackend();
    void processIntent();
    void printSnapshot(const Snapshot& snapshot);
    void refreshStatus();
    void presentCamera();
    void changeView(bool coffee);
    void stopCoffee();
    void report(const char* operation, int error);
    void command(Command command, int argument = 0);
    static uint64_t monotonicNs();
    FactoryBackendLease mBackend;
    bool mBackendReady = false, mCameraPending = false;
    uint64_t mBackendRetryNs = 0;
    Snapshot mSnapshot;
    std::string mPendingAction;
    FactoryDashboard mDashboard;
    lv_obj_t *mHost = nullptr, *mCoffee = nullptr, *mBack = nullptr;
    lv_obj_t *mOverlay = nullptr, *mNotice = nullptr;
    lv_obj_t *mDigitEntry = nullptr, *mDigitCover = nullptr;
    bool mDigitWanted = false;
    lv_timer_t* mTimer = nullptr;
    lv_draw_buf_t* mCanvasBuffer = nullptr;
    lv_display_t* mDisplay = nullptr;
    bool mResumed = false, mCoffeeVisible = false;
    bool mWantCoffee = false, mViewPending = false, mPlayAfterStop = false;
    uint64_t mStatusNs = 0, mOverlayNs = 0;
    uint64_t mLastSequence = 0, mLastCaptureNs = 0, mPresented = 0, mCameraDeltaNs = 0;
    uint64_t mLastPresentationNs = 0;
    uint64_t mRenderStartNs = 0, mPreviousRenderNs = 0;
    uint64_t mDrawNs = 0, mDrawSamples = 0, mIntervalNs = 0, mIntervalSamples = 0;
};
}
