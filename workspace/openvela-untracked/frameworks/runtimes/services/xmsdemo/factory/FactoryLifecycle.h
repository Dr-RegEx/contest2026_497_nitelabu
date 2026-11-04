// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "FactoryBackend.h"
#include <atomic>
#include <pthread.h>

namespace os::app::factory {
class FactoryLifecycle;

// UI-loop-only reservation. Destruction posts retirement, never deletes a backend.
// The executor owns the unique_ptr throughout creation, use and retirement.
class FactoryBackendLease final {
public:
    FactoryBackendLease() = default;
    ~FactoryBackendLease() { reset(); }
    FactoryBackendLease(const FactoryBackendLease&) = delete;
    FactoryBackendLease& operator=(const FactoryBackendLease&) = delete;
    FactoryBackendLease(FactoryBackendLease&& other) noexcept;
    FactoryBackendLease& operator=(FactoryBackendLease&& other) noexcept;
    bool reserved() const { return mOwner != nullptr; }
    bool pending() const; // Separate from errno: start() may itself fail with EAGAIN.
    int status() const; // 0 ready; -EINPROGRESS while pending(); otherwise failure.
    // Latest requested state, applied/retried by the worker in order. Posting
    // pause needs no free queue entry and cannot lose a request to -EAGAIN.
    void setActive(bool active);
    bool active() const; // Latest resume is applied; UI commands may proceed.
    int activeError() const;
    FactoryBackend* get() const;
    explicit operator bool() const { return get() != nullptr; }
    FactoryBackend* operator->() const { return get(); }
    // Caller must finish all backend calls and release every frame before reset.
    void reset() noexcept;
private:
    friend class FactoryLifecycle;
    FactoryLifecycle* mOwner = nullptr;
};

// One fixed reservation, one joinable worker, at most one backend process-wide.
// UI methods are serialized on the shared UI loop; they never wait for the worker.
// Factory and backend lifecycle methods must not throw or call into the UI.
class FactoryLifecycle final {
public:
    using Factory = std::unique_ptr<FactoryBackend> (*)();
    explicit FactoryLifecycle(Factory factory = createFactoryBackend) : mFactory(factory) {}
    // PROCESS TEARDOWN ONLY. All UI leases must be released before teardown can
    // complete. Never invoke this from Activity/Application::onDestroy in Lite.
    ~FactoryLifecycle();
    FactoryLifecycle(const FactoryLifecycle&) = delete;
    FactoryLifecycle& operator=(const FactoryLifecycle&) = delete;
    int initialize(); // Thread failure creates no backend; a later call retries.
    int acquire(FactoryBackendLease& lease); // -EBUSY until prior deletion completes.
private:
    friend class FactoryBackendLease;
    friend FactoryLifecycle& factoryLifecycle();
    enum class Phase : unsigned { Empty, Reserved, Requested, Ready, Failed };
    static_assert(std::atomic<Phase>::is_always_lock_free, "UI handoff must not lock");
    static_assert(std::atomic<bool>::is_always_lock_free, "UI handoff must not lock");
    static_assert(std::atomic<int>::is_always_lock_free, "UI handoff must not lock");
    static_assert(std::atomic<unsigned>::is_always_lock_free, "UI handoff must not lock");
    static void* entry(void* context);
    static void processTeardown();
    void run();
    Factory mFactory;
    pthread_t mThread{};
    bool mThreadStarted = false; // UI-only, then actual process teardown.
    bool mProcessLifetime = false, mExitRegistered = false;
    std::atomic<Phase> mPhase{Phase::Empty};
    std::atomic<bool> mReleased{false}, mStopping{false};
    // Bit 0: desired active value; bit 1: request not yet acknowledged.
    std::atomic<unsigned> mActiveState{0};
    std::atomic<int> mActiveError{0};
    FactoryBackend* mPublished = nullptr; // Published by Ready; borrowed until release.
    int mError = 0; // Published by Failed; immutable until release.
};

// Initialized on the lifecycle/UI thread, never in the Lite launcher task.
// Static storage, with checked process-exit registration before thread creation.
// In Lite its lifetime is the shared system process lifetime.
FactoryLifecycle& factoryLifecycle();
}
