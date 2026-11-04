// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <pthread.h>

namespace os::app::s31product {

// Process-owned controller: callbacks never retain an Activity or LVGL object.
// A single sleeping worker serializes synchronous MEDIA calls. Its 8 KiB stack
// remains reserved after first use; each Player and listener is closed on exit.
class MediaController {
public:
    enum class State { Idle, Opening, Preparing, Playing, Paused, Stopping, Completed, Error };
    struct Snapshot { State state; int error; bool active; bool owned; };
    static MediaController& instance();
    uint64_t newOwner();
    int play(uint64_t owner);
    int pause(uint64_t owner);
    void stop(uint64_t owner);
    Snapshot snapshot(uint64_t owner);
    static const char* name(State state);

private:
    enum class Wanted { Run, Pause, Stop };
    MediaController() = default;
    static void* worker(void* cookie);
    static void event(void* cookie, int event, int result, const char* extra);
    void run();
    bool cancelled();
    void setState(State state);
    pthread_mutex_t mMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t mChanged = PTHREAD_COND_INITIALIZER;
    uint64_t mNextOwner = 0;
    uint64_t mOwner = 0;
    bool mWorker = false;
    bool mActive = false;
    bool mDone = false;
    bool mCompleted = false;
    int mError = 0;
    Wanted mWanted = Wanted::Stop;
    State mState = State::Idle;
};
}
