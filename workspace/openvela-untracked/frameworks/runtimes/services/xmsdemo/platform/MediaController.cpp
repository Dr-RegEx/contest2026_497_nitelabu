// SPDX-License-Identifier: Apache-2.0
#include "MediaController.h"
#include <media_player.h>
#include <cerrno>
#include <cstdio>
#include <limits.h>

namespace os::app::s31product {
namespace {
class Lock {
public:
    explicit Lock(pthread_mutex_t& mutex) : mMutex(mutex) { pthread_mutex_lock(&mMutex); }
    ~Lock() { pthread_mutex_unlock(&mMutex); }
private:
    pthread_mutex_t& mMutex;
};
constexpr const char* kTone = "/etc/media/demo-tone.wav";
}

MediaController& MediaController::instance()
{
    static MediaController controller;
    return controller;
}

const char* MediaController::name(State state)
{
    switch (state) {
    case State::Idle: return "Ready";
    case State::Opening: return "Opening";
    case State::Preparing: return "Preparing";
    case State::Playing: return "Playing";
    case State::Paused: return "Paused";
    case State::Stopping: return "Stopping";
    case State::Completed: return "Finished";
    case State::Error: return "Unavailable";
    }
    return "Unavailable";
}

uint64_t MediaController::newOwner()
{
    Lock lock(mMutex);
    return ++mNextOwner;
}

MediaController::Snapshot MediaController::snapshot(uint64_t owner)
{
    Lock lock(mMutex);
    return {mState, mError, mActive, !mActive || mOwner == owner};
}

int MediaController::play(uint64_t owner)
{
    Lock lock(mMutex);
    if (!owner) return -EINVAL;
    if (mActive) {
        if (mOwner != owner || mWanted == Wanted::Stop || mDone) return -EBUSY;
        mWanted = Wanted::Run;
        pthread_cond_signal(&mChanged);
        return 0;
    }
    if (!mWorker) {
        pthread_attr_t attr;
        int ret = pthread_attr_init(&attr);
        if (ret) return -ret;
        size_t stack = 8192;
#ifndef __NuttX__
        const auto minimum = static_cast<size_t>(PTHREAD_STACK_MIN);
        if (stack < minimum) stack = minimum;
#endif
        ret = pthread_attr_setstacksize(&attr, stack);
        if (!ret) ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_t thread;
        if (!ret) ret = pthread_create(&thread, &attr, worker, this);
        pthread_attr_destroy(&attr);
        if (ret) { mState = State::Error; mError = -ret; return -ret; }
        mWorker = true;
    }
    mOwner = owner;
    mError = 0;
    mDone = mCompleted = false;
    mActive = true;
    mWanted = Wanted::Run;
    mState = State::Opening;
    pthread_cond_signal(&mChanged);
    return 0;
}

int MediaController::pause(uint64_t owner)
{
    Lock lock(mMutex);
    if (!mActive || mOwner != owner || mDone || mWanted == Wanted::Stop)
        return -EINVAL;
    if (mState != State::Playing && mState != State::Paused) return -EAGAIN;
    mWanted = Wanted::Pause;
    pthread_cond_signal(&mChanged);
    return 0;
}

void MediaController::stop(uint64_t owner)
{
    Lock lock(mMutex);
    if (mActive && mOwner == owner) {
        mWanted = Wanted::Stop;
        mState = State::Stopping;
        pthread_cond_signal(&mChanged);
    }
}

bool MediaController::cancelled()
{
    Lock lock(mMutex);
    return mWanted == Wanted::Stop || mDone;
}

void MediaController::setState(State state)
{
    Lock lock(mMutex);
    if (mWanted != Wanted::Stop && !mDone) mState = state;
}

void MediaController::event(void* cookie, int event, int result, const char*)
{
    auto& self = *static_cast<MediaController*>(cookie);
    Lock lock(self.mMutex);
    if (!self.mActive) return;
    std::printf("S31Audio event=%d result=%d\n", event, result);
    if (self.mWanted == Wanted::Stop) return;
    if (result < 0) {
        if (!self.mError) self.mError = result;
        self.mDone = true;
    } else if (event == MEDIA_EVENT_COMPLETED) {
        self.mCompleted = true;
        self.mDone = true;
    } else if (self.mWanted != Wanted::Stop && !self.mDone) {
        if (event == MEDIA_EVENT_STARTED) self.mState = State::Playing;
        if (event == MEDIA_EVENT_PAUSED) self.mState = State::Paused;
    }
    pthread_cond_signal(&self.mChanged);
}

void* MediaController::worker(void* cookie)
{
#ifdef __NuttX__
    pthread_setname_np(pthread_self(), "s31audio");
#endif
    static_cast<MediaController*>(cookie)->run();
    return nullptr;
}

void MediaController::run()
{
    for (;;) {
        {
            Lock lock(mMutex);
            while (!mActive) pthread_cond_wait(&mChanged, &mMutex);
        }
        int ret = 0;
        void* player = nullptr;
        if (!cancelled()) {
            player = media_player_open(MEDIA_STREAM_MUSIC);
            if (!player) ret = -ENODEV;
        }
        if (player && !cancelled()) ret = media_player_set_event_callback(player, this, event);
        if (player && ret >= 0 && !cancelled()) {
            setState(State::Preparing);
            ret = media_player_prepare(player, kTone, nullptr);
        }
        if (player && ret >= 0 && !cancelled()) ret = media_player_start(player);
        bool running = true;
        while (player && ret >= 0) {
            Wanted wanted;
            {
                Lock lock(mMutex);
                while (!mDone && mWanted != Wanted::Stop &&
                       (mWanted == Wanted::Run) == running)
                    pthread_cond_wait(&mChanged, &mMutex);
                if (mDone || mWanted == Wanted::Stop) break;
                wanted = mWanted;
            }
            // No application lock is held across a MEDIA call or close/join.
            ret = wanted == Wanted::Run ? media_player_start(player) : media_player_pause(player);
            running = wanted == Wanted::Run;
        }
        {
            Lock lock(mMutex);
            if (ret < 0 && !mError) mError = ret;
            mWanted = Wanted::Stop;
            mState = State::Stopping;
        }
        // close consumes the handle even on failure and drains its callbacks.
        // No subsequent session starts until this has returned.
        int closed = player ? media_player_close(player, 0) : 0;
        {
            Lock lock(mMutex);
            if (closed < 0 && !mError) mError = closed;
            mState = mError ? State::Error : mCompleted ? State::Completed : State::Idle;
            mActive = false;
            std::printf("S31Audio closed owner=%llu result=%d state=%s\n",
                        static_cast<unsigned long long>(mOwner), mError, name(mState));
        }
    }
}
}
