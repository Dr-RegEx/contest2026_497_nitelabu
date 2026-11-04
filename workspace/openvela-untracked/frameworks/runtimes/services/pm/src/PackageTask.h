#pragma once

#include <uv.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace os {
namespace pm {

// Called only on the owning service loop. The handles retain this worker until
// its child has exited; observers may disappear without freeing a live stage.
class PackageTask : public std::enable_shared_from_this<PackageTask> {
public:
    using Progress = std::function<void(uint8_t)>;
    using Completion = std::function<void(int)>;

    explicit PackageTask(uv_loop_t* loop) : mLoop(loop) {}
    ~PackageTask();
    PackageTask(const PackageTask&) = delete;
    PackageTask& operator=(const PackageTask&) = delete;

    // argv[4] is filled with the child's progress fd. A negative return is a
    // synchronous launch failure and does not also invoke Completion.
    int start(std::vector<std::string> argv, std::shared_ptr<void> lifetime,
              Progress progress, Completion completion);
    void cancel();

private:
    struct PollContext;
    struct TimerContext;
    void onPoll(int status, int events);
    void tick();
    void drain();
    void closeRead();
    void closeHandles();
    void finish(int result, bool notify);

    uv_loop_t* mLoop;
    PollContext* mPoll = nullptr;
    TimerContext* mTimer = nullptr;
    int mReadFd = -1;
    int mWriteFd = -1;
    int mPid = -1;
    int mExitStatus = 0;
    int mError = 0;
    int mTerminal = -1;
    uint64_t mStartedAt = 0;
    uint64_t mTerminatedAt = 0;
    bool mStarted = false;
    bool mFinished = false;
    bool mPipeEnded = false;
    bool mExited = false;
    bool mNeedTerminate = false;
    bool mSentTerm = false;
    bool mSentKill = false;
    std::shared_ptr<void> mLifetime;
    Progress mProgress;
    Completion mCompletion;
};

} // namespace pm
} // namespace os
