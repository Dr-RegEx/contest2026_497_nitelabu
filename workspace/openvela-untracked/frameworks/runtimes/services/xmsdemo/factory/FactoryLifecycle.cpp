// SPDX-License-Identifier: Apache-2.0
#include "FactoryLifecycle.h"
#include <cerrno>
#include <cstdlib>
#include <limits.h>
#include <new>
#include <poll.h>

namespace os::app::factory {
namespace {
// A timed sleep avoids a fallible wakeup/queue operation on the UI retirement
// path. This is a polling interval, NOT a bound on shutdown or scheduling time.
void idle() { poll(nullptr, 0, 20); }
FactoryLifecycle*& processOwner()
{
    // FLAT builtins share static storage even across SystemServer restarts.
    static FactoryLifecycle* owner = nullptr;
    return owner;
}
}

FactoryBackendLease::FactoryBackendLease(FactoryBackendLease&& other) noexcept
    : mOwner(other.mOwner)
{
    other.mOwner = nullptr;
}

FactoryBackendLease& FactoryBackendLease::operator=(FactoryBackendLease&& other) noexcept
{
    if (this != &other) {
        reset();
        mOwner = other.mOwner;
        other.mOwner = nullptr;
    }
    return *this;
}

void FactoryBackendLease::reset() noexcept
{
    auto owner = mOwner;
    mOwner = nullptr;
    // Capacity was reserved by acquire(). No allocation, mutex, queue push,
    // thread creation, shutdown, deletion or join is possible here.
    if (owner) owner->mReleased.store(true, std::memory_order_release);
}

bool FactoryBackendLease::pending() const
{
    if (!mOwner) return false;
    auto phase = mOwner->mPhase.load(std::memory_order_acquire);
    return phase == FactoryLifecycle::Phase::Reserved || phase == FactoryLifecycle::Phase::Requested;
}

int FactoryBackendLease::status() const
{
    if (!mOwner) return -ENODEV;
    auto phase = mOwner->mPhase.load(std::memory_order_acquire);
    if (phase == FactoryLifecycle::Phase::Ready) return 0;
    if (phase == FactoryLifecycle::Phase::Failed) return mOwner->mError;
    return -EINPROGRESS;
}

FactoryBackend* FactoryBackendLease::get() const
{
    return status() == 0 ? mOwner->mPublished : nullptr;
}

void FactoryBackendLease::setActive(bool active)
{
    if (mOwner) mOwner->mActiveState.store(active ? 3U : 2U, std::memory_order_release);
}

bool FactoryBackendLease::active() const
{
    return mOwner && mOwner->mActiveState.load(std::memory_order_acquire) == 1U;
}

int FactoryBackendLease::activeError() const
{
    return mOwner ? mOwner->mActiveError.load(std::memory_order_acquire) : -ENODEV;
}

int FactoryLifecycle::initialize()
{
    if (mStopping.load(std::memory_order_acquire)) return -ESHUTDOWN;
    if (mThreadStarted) return 0;
    if (mProcessLifetime && !mExitRegistered) {
        // NuttX has a finite exit-handler table. Compiler-generated registration
        // of a nontrivial function-static destructor does not check failure.
        // No worker/backend may exist unless process teardown is registered.
        if (std::atexit(processTeardown) != 0) return -ENOMEM;
        mExitRegistered = true;
    }
    pthread_attr_t attributes;
    int ret = pthread_attr_init(&attributes);
    if (ret) return -ret;
    size_t stack = 8192;
    if (stack < static_cast<size_t>(PTHREAD_STACK_MIN)) stack = PTHREAD_STACK_MIN;
    ret = pthread_attr_setstacksize(&attributes, stack);
    if (!ret) ret = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE);
    if (!ret) ret = pthread_create(&mThread, &attributes, entry, this);
    pthread_attr_destroy(&attributes);
    if (!ret) mThreadStarted = true;
    return -ret;
}

int FactoryLifecycle::acquire(FactoryBackendLease& lease)
{
    if (lease.reserved()) return -EALREADY;
    int ret = initialize();
    if (ret) return ret;
    auto empty = Phase::Empty;
    if (!mPhase.compare_exchange_strong(empty, Phase::Reserved,
                                       std::memory_order_acq_rel)) return -EBUSY;
    mReleased.store(false, std::memory_order_relaxed);
    mActiveState.store(0, std::memory_order_relaxed);
    mActiveError.store(0, std::memory_order_relaxed);
    lease.mOwner = this;
    mPhase.store(Phase::Requested, std::memory_order_release);
    return 0;
}

void* FactoryLifecycle::entry(void* context)
{
    static_cast<FactoryLifecycle*>(context)->run();
    return nullptr;
}

void FactoryLifecycle::run()
{
    std::unique_ptr<FactoryBackend> backend;
    bool active = false;
    for (;;) {
        auto phase = mPhase.load(std::memory_order_acquire);
        if (phase == Phase::Empty) {
            if (mStopping.load(std::memory_order_acquire)) return;
        } else if (phase != Phase::Reserved) {
            if (mReleased.load(std::memory_order_acquire)) {
                // The UI has stopped using the object and released its frame.
                // Keep the slot occupied through shutdown AND the destructor
                // (NativeBackend's destructor calls shutdown again).
                if (backend) { backend->shutdown(); backend.reset(); }
                mPublished = nullptr;
                mPhase.store(Phase::Empty, std::memory_order_release);
                continue;
            }
            if (phase == Phase::Requested) {
                int ret = -ESHUTDOWN;
                if (!mStopping.load(std::memory_order_acquire)) {
                    backend = mFactory();
                    ret = backend ? backend->start() : -ENOMEM;
                    // Establish inactive state before exposing the provider
                    // to an Activity, including providers with active defaults.
                    if (!ret) {
                        do {
                            ret = backend->setActive(false);
                            if (ret == -EAGAIN) idle();
                        } while (ret == -EAGAIN &&
                                 !mReleased.load(std::memory_order_acquire) &&
                                 !mStopping.load(std::memory_order_acquire));
                    }
                }
                if (ret) {
                    // Includes partial start and allocation failure. Cleanup
                    // must finish before publishing failure or freeing capacity.
                    if (backend) { backend->shutdown(); backend.reset(); }
                    mError = ret;
                    mPhase.store(Phase::Failed, std::memory_order_release);
                } else {
                    active = false;
                    mPublished = backend.get();
                    mPhase.store(Phase::Ready, std::memory_order_release);
                }
                continue;
            }
            if (phase == Phase::Ready) {
                unsigned request = mActiveState.load(std::memory_order_acquire);
                if (request & 2U) {
                    const bool wanted = request & 1U;
                    const int ret = wanted != active ? backend->setActive(wanted) : 0;
                    mActiveError.store(ret, std::memory_order_release);
                    if (!ret) {
                        active = wanted;
                        // A changed desired state remains pending. UI commands
                        // stay gated until the latest resume is acknowledged.
                        mActiveState.compare_exchange_strong(request, wanted ? 1U : 0U,
                                                             std::memory_order_acq_rel);
                    }
                    // Only this worker applies state, so an older pause can
                    // never run after a newer resume has been applied. Failed
                    // requests retry; intermediate states may be coalesced.
                }
            }
        }
        idle();
    }
}

FactoryLifecycle::~FactoryLifecycle()
{
    mStopping.store(true, std::memory_order_release);
    // Never forcibly delete a borrowed object. An unreleased lease deliberately
    // prevents process teardown from completing rather than allowing a UAF.
    if (mThreadStarted && pthread_join(mThread, nullptr) != 0) std::abort();
}

FactoryLifecycle& factoryLifecycle()
{
    alignas(FactoryLifecycle) static unsigned char storage[sizeof(FactoryLifecycle)];
    auto& owner = processOwner();
    if (!owner) {
        owner = new (storage) FactoryLifecycle;
        owner->mProcessLifetime = true;
    }
    return *owner;
}

void FactoryLifecycle::processTeardown()
{
    // Invoked only by actual owning-process exit, never by Application cleanup.
    auto& owner = processOwner();
    if (owner) {
        owner->~FactoryLifecycle();
        owner = nullptr;
    }
}
}
