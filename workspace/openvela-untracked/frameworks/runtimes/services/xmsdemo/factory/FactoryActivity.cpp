// SPDX-License-Identifier: Apache-2.0
#include "FactoryActivity.h"
#include "CoffeeWindow.h"
#include <BaseWindow.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <time.h>

namespace os::app::factory {
namespace {
lv_obj_t* panel(lv_obj_t* parent)
{
    auto obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, 800, 480);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
void setDisabled(lv_obj_t* obj, bool disabled)
{
    if (disabled) lv_obj_add_state(obj, LV_STATE_DISABLED);
    else lv_obj_remove_state(obj, LV_STATE_DISABLED);
}
uint32_t stateColor(const Status& status)
{
    if (status.state == State::Error) return 0x582026;
    if (status.state == State::Ready) return 0x124b38;
    if (status.state == State::Busy || status.state == State::Starting) return 0x5a4412;
    return 0x17426a;
}
void milliseconds(char* output, size_t size, uint64_t ns, uint64_t count)
{
    if (!count) { std::snprintf(output, size, "--"); return; }
    uint64_t tenths = ns / count / 100000;
    std::snprintf(output, size, "%llu.%llu", static_cast<unsigned long long>(tenths / 10),
                  static_cast<unsigned long long>(tenths % 10));
}
}

uint64_t FactoryActivity::monotonicNs()
{
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
}

void FactoryActivity::onCreate()
{
    mPendingAction = getIntent().mAction;
    auto root = static_cast<lv_obj_t*>(getWindow()->getRoot());
    if (!root) { finish(); return; }
    mHost = panel(root);
    lv_obj_center(mHost);
    mCanvasBuffer = lv_draw_buf_create(240, 240, LV_COLOR_FORMAT_RGB565, 0);
    mDashboard.camera_canvas_draw_buf = mCanvasBuffer;
    mDashboard.callback = action;
    mDashboard.context = this;
    factory_dashboard_create(&mDashboard, mHost);
    mCoffee = panel(mHost);
    lv_obj_add_flag(mCoffee, LV_OBJ_FLAG_HIDDEN);

    // A sibling above all coffee panels preserves the upstream Factory shortcut
    // on every generated screen without altering the generated screen contents.
    mBack = lv_button_create(mHost);
    lv_obj_set_size(mBack, 120, 40);
    lv_obj_align(mBack, LV_ALIGN_BOTTOM_RIGHT, -16, -16);
    lv_obj_set_style_radius(mBack, 20, 0);
    lv_obj_set_style_bg_color(mBack, lv_color_hex(0x303030), 0);
    lv_obj_set_style_bg_opa(mBack, LV_OPA_70, 0);
    lv_obj_set_style_border_width(mBack, 1, 0);
    lv_obj_set_style_border_color(mBack, lv_color_hex(0xff8a5b), 0);
    lv_obj_set_style_shadow_width(mBack, 0, 0);
    lv_obj_add_event_cb(mBack, factoryClicked, LV_EVENT_CLICKED, this);
    auto backLabel = lv_label_create(mBack);
    lv_label_set_text(backLabel, "Factory");
    lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_18, 0);
    lv_obj_center(backLabel);
    lv_obj_add_flag(mBack, LV_OBJ_FLAG_HIDDEN);

    mNotice = lv_label_create(mHost);
    lv_obj_set_width(mNotice, 752);
    lv_obj_align(mNotice, LV_ALIGN_TOP_LEFT, 24, 94);
    lv_label_set_long_mode(mNotice, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(mNotice, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mNotice, lv_color_hex(0xffb047), 0);
    lv_label_set_text(mNotice, "");
    mOverlay = lv_label_create(mHost);
    lv_obj_align(mOverlay, LV_ALIGN_TOP_RIGHT, -4, 1);
    lv_obj_set_style_text_font(mOverlay, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mOverlay, lv_color_white(), 0);
    lv_obj_set_style_bg_color(mOverlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(mOverlay, LV_OPA_70, 0);
    lv_label_set_text(mOverlay, "Display draw -- ms | interval -- ms | camera -- ms");
#ifdef CONFIG_ESP32S31_VISION
    mDigitEntry = lv_button_create(mHost);
    lv_obj_set_pos(mDigitEntry, 288, 294);
    lv_obj_set_size(mDigitEntry, 200, 46);
    auto digitLabel = lv_label_create(mDigitEntry);
    lv_label_set_text(digitLabel, "Digit recognition");
    lv_obj_set_style_text_font(digitLabel, &lv_font_montserrat_18, 0);
    lv_obj_center(digitLabel);
    lv_obj_add_event_cb(mDigitEntry, digitClicked, LV_EVENT_CLICKED, this);
    mDigitCover = panel(mHost);
    lv_obj_set_style_bg_color(mDigitCover, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(mDigitCover, LV_OPA_COVER, 0);
    lv_obj_add_flag(mDigitCover, LV_OBJ_FLAG_CLICKABLE);
    auto transition = lv_label_create(mDigitCover);
    lv_label_set_text(transition, "Opening original digit demo...");
    lv_obj_center(transition);
    auto back = lv_button_create(mDigitCover);
    lv_obj_set_pos(back, 646, 438);
    lv_obj_set_size(back, 148, 40);
    auto label = lv_label_create(back);
    lv_label_set_text(label, "Return");
    lv_obj_center(label);
    lv_obj_add_event_cb(back, returnClicked, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(mDigitCover, LV_OBJ_FLAG_HIDDEN);
#endif
    mDisplay = lv_obj_get_display(root);
    // Creation/startup and retirement belong to the process executor.
    // Acquire only while resumed; a pending startup is cancellable by destroy.
    if (!mCanvasBuffer) report("Preview memory", -ENOMEM);
    refreshStatus();
}

void FactoryActivity::onNewIntent(const Intent& intent)
{
    if (!mPendingAction.empty()) {
        std::printf("S31Factory action=busy result=%d\n", -EBUSY);
        return;
    }
    mPendingAction = intent.mAction;
}

void FactoryActivity::printSnapshot(const Snapshot& s)
{
    std::printf("S31Factory snapshot ns=%llu camera=%u camera_error=%d captured=%llu presented=%llu "
                "audio=%u audio_error=%d bytes=%llu has_recording=%u "
                "storage=%u storage_error=%d mounted=%u probe=%u probes=%llu "
                "usb=%u usb_error=%d usb_inputs=%llu led=%d coffee=%u\n",
        static_cast<unsigned long long>(s.sampledNs), static_cast<unsigned>(s.camera.state),
        s.camera.error, static_cast<unsigned long long>(s.capturedFrames),
        static_cast<unsigned long long>(mPresented), static_cast<unsigned>(s.audioState),
        s.audio.error, static_cast<unsigned long long>(s.recordedBytes), unsigned(s.hasRecording),
        static_cast<unsigned>(s.storage.state), s.storage.error, unsigned(s.sdMounted),
        unsigned(s.sdProbePassed), static_cast<unsigned long long>(s.sdProbeCount),
        static_cast<unsigned>(s.usb.state), s.usb.error,
        static_cast<unsigned long long>(s.usbInputCount), s.activeLedButton, unsigned(mCoffeeVisible));
}

void FactoryActivity::processIntent()
{
    if (mPendingAction.empty() || !mResumed) return;
    const std::string action = mPendingAction;
    if (action == "s31.factory.digit" || action == "s31.factory.return") {
        if (!mBackend || !mBackend.active()) return;
        enterDigit(action == "s31.factory.digit");
        mPendingAction.clear();
        return;
    }
    if (action == "s31.factory.close") {
        mPendingAction.clear();
        std::printf("S31Factory action=close result=0\n");
        finish();
        return;
    }
    if (action == "s31.factory.coffee" || action == "s31.factory.dashboard") {
        const bool coffee = action == "s31.factory.coffee";
        changeView(coffee);
        mPendingAction.clear();
        std::printf("S31Factory action=%s result=%d\n", action.c_str(),
                    mCoffeeVisible == coffee ? 0 : -EIO);
        return;
    }
    Command selected = Command::StopAudio;
    int argument = 0;
    const bool snapshot = action == "s31.factory.snapshot";
    if (action == "s31.factory.storage") selected = Command::ProbeStorage;
    else if (action == "s31.factory.record") selected = Command::StartRecording;
    else if (action == "s31.factory.stop") selected = Command::StopAudio;
    else if (action == "s31.factory.play") selected = Command::PlayRecording;
    else if (action == "s31.factory.reset-audio") selected = Command::ResetAudio;
    else if (action.size() == 16 && action.compare(0, 15, "s31.factory.led") == 0 &&
             action[15] >= '0' && action[15] <= '3') {
        selected = Command::SelectLed;
        argument = action[15] - '0';
    } else if (!snapshot) {
        mPendingAction.clear();
        // AM's ordinary launch action is intentionally not a factory command.
        if (action.compare(0, 12, "s31.factory.") == 0)
            std::printf("S31Factory action=unknown result=%d\n", -EINVAL);
        return;
    }
    if (!mBackend || !mBackend.active()) return;
    int ret;
    if (snapshot) {
        Snapshot fresh;
        ret = mBackend->snapshot(fresh);
        if (!ret) printSnapshot(fresh);
    } else {
        ret = mBackend->submit(selected, argument);
    }
    if (ret == -EAGAIN) return;
    mPendingAction.clear();
    // A queued command is acknowledged here; completion requires a later
    // snapshot containing the actual device/service result.
    std::printf("S31Factory action=%s result=%d\n", action.c_str(), ret);
}

void FactoryActivity::onResume()
{
    if (!mHost || mResumed) return;
    mResumed = true;
    mPreviousRenderNs = mRenderStartNs = mDrawNs = mDrawSamples = 0;
    mIntervalNs = mIntervalSamples = mCameraDeltaNs = 0;
    mLastCaptureNs = mLastPresentationNs = 0;
    mStatusNs = mOverlayNs = 0;
    lv_display_add_event_cb(mDisplay, displayEvent, LV_EVENT_ALL, this);
    mBackendRetryNs = 0;
    if (mCoffeeVisible) changeView(true);
    mTimer = lv_timer_create(tick, 16, this);
    if (!mTimer) { report("UI timer", -ENOMEM); return; }
    mBackend.setActive(true);
    refresh();
}

void FactoryActivity::onPause()
{
    mResumed = false;
    if (mTimer) { lv_timer_delete(mTimer); mTimer = nullptr; }
    if (mDisplay) lv_display_remove_event_cb_with_user_data(mDisplay, displayEvent, this);
    mRenderStartNs = mPreviousRenderNs = 0;
    mPlayAfterStop = false;
    mViewPending = false;
    stopCoffee(); // Cancel delayed navigation and infinite loops before hiding.
    // The worker retries pause even after the UI polling timer is gone. Keep
    // this backend and its RAM recording for a later resume of this Activity.
    mBackend.setActive(false);
    mDigitWanted = false;
    if (mDigitCover) lv_obj_add_flag(mDigitCover, LV_OBJ_FLAG_HIDDEN);
}

void FactoryActivity::onDestroy()
{
    onPause();
    // All frame leases ended in presentCamera() on this same UI loop. Releasing
    // this fixed reservation cannot allocate, reject, wait, join or delete.
    mBackend.reset();
    mBackendReady = mCameraPending = false;
    if (mHost) { lv_obj_delete(mHost); mHost = nullptr; }
    // Canvas objects must be deleted before their pixel allocation is released.
    if (mCanvasBuffer) { lv_draw_buf_destroy(mCanvasBuffer); mCanvasBuffer = nullptr; }
    mCoffee = mBack = mOverlay = mNotice = nullptr;
    mDisplay = nullptr;
    mDashboard = {};
}

void FactoryActivity::report(const char* operation, int error)
{
    if (error && mNotice) lv_label_set_text_fmt(mNotice, "%s: error %d", operation, error);
}

void FactoryActivity::command(Command cmd, int argument)
{
    if (!mResumed || !mBackend) { report("Command", -ENODEV); return; }
    if (!mBackend.active()) {
        mPlayAfterStop = false;
        report("Devices resuming", -EAGAIN);
        return;
    }
    int ret = mBackend->submit(cmd, argument);
    if (ret) { mPlayAfterStop = false; report("Command", ret); }
    // Submission is deliberately never displayed as a completed hardware action.
}

void FactoryActivity::action(void* context, DashboardAction selected, int argument)
{
    auto self = static_cast<FactoryActivity*>(context);
    if (!self->mResumed) return;
    switch (selected) {
    case DashboardAction::Coffee:
        self->mWantCoffee = true;
        self->mViewPending = true;
        break;
    case DashboardAction::ProbeStorage:
        self->command(Command::ProbeStorage);
        break;
    case DashboardAction::SelectLed:
        self->command(Command::SelectLed, argument);
        break;
    case DashboardAction::AudioToggle:
        // Query a fresh coherent snapshot so repeated taps do not act on stale
        // recording state. Backend must also serialize/reject conflicting work.
        if (!self->mBackend || self->mBackend->snapshot(self->mSnapshot)) return;
        switch (self->mSnapshot.audioState) {
        case AudioState::Idle:
            self->mPlayAfterStop = false;
            self->command(Command::StartRecording);
            break;
        case AudioState::Recording:
            self->mPlayAfterStop = true;
            self->command(Command::StopAudio);
            break;
        case AudioState::Playing:
            self->mPlayAfterStop = false;
            self->command(Command::StopAudio);
            break;
        case AudioState::Error:
            self->mPlayAfterStop = false;
            self->command(Command::ResetAudio);
            break;
        default: break;
        }
        break;
    }
}

void FactoryActivity::enterDigit(bool enter)
{
    if (!mBackend || !mBackend.active() || !mDigitCover) return;
    int ret = mBackend->submit(enter ? Command::StartDigit : Command::StopDigit);
    if (ret) { report("Digit transition", ret); return; }
    mDigitWanted = enter;
    if (enter) {
        stopCoffee();
        lv_obj_remove_flag(mDigitCover, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(mDigitCover);
    }
    // Return stays visible until the worker acknowledges camera/LCD retirement.
}
void FactoryActivity::digitClicked(lv_event_t* event)
{
    static_cast<FactoryActivity*>(lv_event_get_user_data(event))->enterDigit(true);
}
void FactoryActivity::returnClicked(lv_event_t* event)
{
    static_cast<FactoryActivity*>(lv_event_get_user_data(event))->enterDigit(false);
}

void FactoryActivity::factoryClicked(lv_event_t* event)
{
    auto self = static_cast<FactoryActivity*>(lv_event_get_user_data(event));
    self->mWantCoffee = false;
    self->mViewPending = true;
}

void FactoryActivity::stopCoffee()
{
    if (coffee_window_owns(mCoffee)) coffee_window_destroy();
}

void FactoryActivity::changeView(bool coffee)
{
    if (coffee) {
        if (!coffee_window_owns(mCoffee)) {
            int ret = coffee_window_create(mCoffee);
            if (ret) {
                std::printf("S31Factory coffee_create error=%d\n", ret);
                report("Coffee assets/UI", ret);
                coffee = false;
            }
        }
    } else stopCoffee();
    mCoffeeVisible = coffee;
    if (mDigitEntry) {
        if (coffee) lv_obj_add_flag(mDigitEntry, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(mDigitEntry, LV_OBJ_FLAG_HIDDEN);
    }
    if (coffee) {
        lv_obj_add_flag(mDashboard.factory_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(mCoffee, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(mBack, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(mNotice, "");
    } else {
        lv_obj_remove_flag(mDashboard.factory_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mCoffee, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mBack, LV_OBJ_FLAG_HIDDEN);
    }
    mLastCaptureNs = mLastPresentationNs = mCameraDeltaNs = 0;
    mCameraPending = true;
}

void FactoryActivity::tick(lv_timer_t* timer)
{
    static_cast<FactoryActivity*>(lv_timer_get_user_data(timer))->refresh();
}

void FactoryActivity::presentCamera()
{
    if (!mBackend || !mBackend.active() || !mCanvasBuffer || mCoffeeVisible) return;
    CameraFrame frame;
    int ret = mBackend->acquireFrame(frame);
    if (ret) { if (ret != -EAGAIN) report("Camera frame", ret); return; }
    uint64_t now = monotonicNs();
    bool valid = frame.data && frame.lease && frame.width == 240 && frame.height == 240 &&
                 frame.stride >= 480 && frame.stride <= frame.size / 240 &&
                 frame.captureNs && now && frame.captureNs <= now;
    bool updated = valid && (frame.sequence != mLastSequence || !mPresented);
    if (updated) {
        for (uint32_t y = 0; y < 240; ++y)
            std::memcpy(mCanvasBuffer->data + y * mCanvasBuffer->header.stride,
                        frame.data + y * frame.stride, 480);
        lv_draw_buf_flush_cache(mCanvasBuffer, nullptr);
        mCameraDeltaNs = mLastCaptureNs && frame.captureNs > mLastCaptureNs ?
                         frame.captureNs - mLastCaptureNs : 0;
        mLastCaptureNs = frame.captureNs;
        mLastSequence = frame.sequence;
        mLastPresentationNs = now;
        ++mPresented;
    } else if (!valid) report("Camera frame format/time", -EINVAL);
    if (frame.lease) mBackend->releaseFrame(frame.lease);
    if (updated) lv_obj_invalidate(mDashboard.camera_canvas);
}

void FactoryActivity::refreshStatus()
{
    auto& view = mDashboard;
    const auto& state = mSnapshot;
    lv_label_set_text_fmt(view.camera_status_label,
        "%s\n%ux%u\nCaptured: %llu\nPresented: %llu\nDropped: %llu",
        state.camera.text, static_cast<unsigned>(state.cameraWidth), static_cast<unsigned>(state.cameraHeight),
        static_cast<unsigned long long>(state.capturedFrames), static_cast<unsigned long long>(mPresented),
        static_cast<unsigned long long>(state.cameraDroppedFrames));
    const char* audioButton = "Record";
    if (state.audioState == AudioState::Recording || state.audioState == AudioState::Playing)
        audioButton = "Stop";
    else if (state.audioState == AudioState::Error) audioButton = "Reset";
    lv_label_set_text(view.audio_btn_label, audioButton);
    setDisabled(lv_obj_get_parent(view.audio_btn_label),
                state.audioState == AudioState::Unavailable || state.audioState == AudioState::Stopping);
    lv_label_set_text(view.audio_status_label, state.audio.text);
    // Avoid multiplication overflow on a malformed or very long backend stream.
    unsigned percent = state.audioCapacityBytes ? static_cast<unsigned>(
        (state.recordedBytes >= state.audioCapacityBytes ? 100 :
         state.recordedBytes / (state.audioCapacityBytes / 100 + 1))) : 0;
    lv_bar_set_value(view.audio_progress, percent, LV_ANIM_OFF);
    lv_label_set_text(view.storage_status_label, state.storage.text);
    lv_obj_set_style_bg_color(view.storage_card, lv_color_hex(state.storage.state == State::Ready && !state.sdProbePassed ?
        0x17426a : stateColor(state.storage)), 0);
    lv_obj_set_style_bg_color(view.usb_hid_card, lv_color_hex(stateColor(state.usb)), 0);
    lv_label_set_text_fmt(view.usb_hid_status_label, "%s: %s", state.usbDevice, state.usb.text);
    lv_label_set_text_fmt(view.usb_hid_input_label, "Input:%llu  %s",
                         static_cast<unsigned long long>(state.usbInputCount), state.usbLastInput);
    for (int i = 0; i < 4; ++i) {
        auto button = view.button_led_buttons[i];
        if (state.activeLedButton == i) lv_obj_add_state(button, LV_STATE_CHECKED);
        else lv_obj_remove_state(button, LV_STATE_CHECKED);
        if (state.pressed[i]) lv_obj_add_state(button, LV_STATE_PRESSED);
        else lv_obj_remove_state(button, LV_STATE_PRESSED);
        setDisabled(button, state.led.state == State::Unavailable);
    }
    lv_label_set_text_fmt(view.button_status_label, "%s | LED: %s", state.buttons.text, state.led.text);
}

void FactoryActivity::pollBackend()
{
    const uint64_t now = monotonicNs();
    if (!mBackend.reserved()) {
        if (now && mBackendRetryNs && now < mBackendRetryNs) return;
        int ret = factoryLifecycle().acquire(mBackend);
        if (ret) {
            mBackendRetryNs = now + 1000000000ULL;
            if (ret == -EBUSY)
                lv_label_set_text(mNotice, "Waiting for previous factory session to close");
            else report("Backend executor", ret);
            return;
        }
        mBackend.setActive(true);
        lv_label_set_text(mNotice, "Starting factory devices");
    }
    if (mBackend.pending()) return;
    int ret = mBackend.status();
    if (ret) {
        report("Backend start", ret);
        mBackend.reset();
        mBackendRetryNs = now + 1000000000ULL;
        return;
    }
    if (!mBackendReady) {
        mBackendReady = mCameraPending = true;
        mSnapshot = {};
        mLastSequence = mLastCaptureNs = mPresented = mCameraDeltaNs = 0;
        mLastPresentationNs = 0;
        lv_label_set_text(mNotice, "");
    }
    ret = mBackend.activeError();
    if (ret && ret != -EAGAIN) report("Active state", ret);
    if (mCameraPending) {
        ret = mBackend->setCameraPreview(!mCoffeeVisible && mCanvasBuffer);
        if (!ret) mCameraPending = false;
        else if (ret != -EAGAIN) report("Camera", ret);
    }
}

void FactoryActivity::refresh()
{
    if (!mResumed) return;
    pollBackend();
    processIntent();
    if (!mResumed) return;
    if (mViewPending) { mViewPending = false; changeView(mWantCoffee); }
    presentCamera();
    uint64_t now = monotonicNs();
    if (!mStatusNs || now - mStatusNs >= 100000000ULL) {
        if (mBackend) {
            int ret = mBackend->snapshot(mSnapshot);
            if (ret && ret != -EAGAIN) report("Status", ret);
        }
        if (mDigitCover && !mDigitWanted && !mSnapshot.digitDisplayOwned &&
            (mSnapshot.digit.state == State::Idle || mSnapshot.digit.state == State::Error) &&
            !lv_obj_has_flag(mDigitCover, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(mDigitCover, LV_OBJ_FLAG_HIDDEN);
            lv_obj_invalidate(mHost);
            std::printf("S31Factory vision returned to dashboard\n");
        }
        if (mSnapshot.audioState == AudioState::Error) mPlayAfterStop = false;
        if (mPlayAfterStop && mSnapshot.audioState == AudioState::Idle) {
            mPlayAfterStop = false;
            if (mSnapshot.hasRecording) command(Command::PlayRecording);
        }
        if (!mCoffeeVisible) refreshStatus();
        mStatusNs = now;
    }
    if (!mOverlayNs || now - mOverlayNs >= 1000000000ULL) {
        char draw[24], interval[24], camera[24];
        milliseconds(draw, sizeof(draw), mDrawNs, mDrawSamples);
        milliseconds(interval, sizeof(interval), mIntervalNs, mIntervalSamples);
        // Stale or first camera frames have no current interval measurement.
        bool fresh = mLastPresentationNs && now >= mLastPresentationNs &&
                     now - mLastPresentationNs < 1000000000ULL;
        milliseconds(camera, sizeof(camera), mCameraDeltaNs, fresh && mCameraDeltaNs ? 1 : 0);
        lv_label_set_text_fmt(mOverlay, "Display draw %s ms | interval %s ms | camera %s ms", draw, interval, camera);
        mDrawNs = mDrawSamples = mIntervalNs = mIntervalSamples = 0;
        mOverlayNs = now;
    }
}

void FactoryActivity::displayEvent(lv_event_t* event)
{
    auto self = static_cast<FactoryActivity*>(lv_event_get_user_data(event));
    if (!self->mResumed) return;
    uint64_t now = monotonicNs();
    if (!now) return;
    if (lv_event_get_code(event) == LV_EVENT_RENDER_START) {
        self->mRenderStartNs = now;
        if (self->mPreviousRenderNs && now >= self->mPreviousRenderNs) {
            self->mIntervalNs += now - self->mPreviousRenderNs;
            ++self->mIntervalSamples;
        }
        self->mPreviousRenderNs = now;
    } else if (lv_event_get_code(event) == LV_EVENT_RENDER_READY && self->mRenderStartNs) {
        if (now >= self->mRenderStartNs) {
            self->mDrawNs += now - self->mRenderStartNs;
            ++self->mDrawSamples;
        }
        self->mRenderStartNs = 0;
    }
}
}
