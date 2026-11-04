#include "PackageTask.h"

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <new>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef CONFIG_SYSTEM_PACKAGE_SERVICE_TASK_TIMEOUT_MS
#define CONFIG_SYSTEM_PACKAGE_SERVICE_TASK_TIMEOUT_MS 60000
#endif

namespace os {
namespace pm {

struct PackageTask::PollContext {
    uv_poll_t handle;
    std::shared_ptr<PackageTask> owner;
};

struct PackageTask::TimerContext {
    uv_timer_t handle;
    std::shared_ptr<PackageTask> owner;
};

PackageTask::~PackageTask() {
    closeHandles();
    if (mReadFd >= 0) close(mReadFd);
    if (mWriteFd >= 0) close(mWriteFd);
}

int PackageTask::start(std::vector<std::string> argv, std::shared_ptr<void> lifetime,
                       Progress progress, Completion completion) {
    if (mStarted || mFinished || argv.size() < 5 || !mLoop) return -EINVAL;
    mStarted = true;
    mLifetime = std::move(lifetime);
    mProgress = std::move(progress);
    mCompletion = std::move(completion);
    auto fail = [this](int error) {
        finish(error, false);
        return error;
    };

    int pipefd[2];
    if (pipe(pipefd) != 0) return fail(-errno);
    mReadFd = pipefd[0];
    mWriteFd = pipefd[1];
    if (fcntl(mReadFd, F_SETFD, FD_CLOEXEC) < 0 ||
        fcntl(mWriteFd, F_SETFD, FD_CLOEXEC) < 0) return fail(-errno);

    auto self = shared_from_this();
    auto poll = new (std::nothrow) PollContext{{}, self};
    if (!poll) return fail(-ENOMEM);
    int ret = uv_poll_init(mLoop, &poll->handle, mReadFd);
    if (ret != 0) {
        delete poll;
        return fail(ret);
    }
    mPoll = poll;
    poll->handle.data = poll;
    ret = uv_poll_start(&poll->handle, UV_READABLE | UV_DISCONNECT,
                        [](uv_poll_t* handle, int status, int events) {
        auto owner = static_cast<PollContext*>(handle->data)->owner;
        owner->onPoll(status, events);
    });
    if (ret != 0) return fail(ret);

    auto timer = new (std::nothrow) TimerContext{{}, self};
    if (!timer) return fail(-ENOMEM);
    ret = uv_timer_init(mLoop, &timer->handle);
    if (ret != 0) {
        delete timer;
        return fail(ret);
    }
    mTimer = timer;
    timer->handle.data = timer;
    ret = uv_timer_start(&timer->handle, [](uv_timer_t* handle) {
        auto owner = static_cast<TimerContext*>(handle->data)->owner;
        owner->tick();
    }, 10, 10);
    if (ret != 0) return fail(ret);

    // Always duplicate to a different descriptor, clearing FD_CLOEXEC in the
    // child even on systems whose dup2(fd, fd) is a no-op.
    const int childFd = mWriteFd == 3 ? 4 : 3;
    argv[4] = std::to_string(childFd);
    std::vector<char*> args;
    for (auto& arg : argv) args.push_back(const_cast<char*>(arg.c_str()));
    args.push_back(nullptr);
    posix_spawnattr_t attr;
    ret = posix_spawnattr_init(&attr);
    if (ret != 0) return fail(-ret);
    ret = posix_spawnattr_setstacksize(&attr, CONFIG_SYSTEM_SERVER_STACKSIZE);
    if (ret != 0) {
        posix_spawnattr_destroy(&attr);
        return fail(-ret);
    }
    posix_spawn_file_actions_t actions;
    ret = posix_spawn_file_actions_init(&actions);
    if (ret != 0) {
        posix_spawnattr_destroy(&attr);
        return fail(-ret);
    }
    ret = posix_spawn_file_actions_addclose(&actions, mReadFd);
    if (ret == 0) ret = posix_spawn_file_actions_adddup2(&actions, mWriteFd, childFd);
    if (ret == 0) ret = posix_spawn_file_actions_addclose(&actions, mWriteFd);
    pid_t pid = -1;
    uv_update_time(mLoop);
    mStartedAt = uv_now(mLoop);
    if (ret == 0) ret = posix_spawn(&pid, args[0], &actions, &attr, args.data(), nullptr);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    close(mWriteFd);
    mWriteFd = -1;
    if (ret != 0) return fail(ret > 0 ? -ret : ret);
    if (pid <= 0) return fail(-ECHILD);
    mPid = pid;
    return pid;
}

void PackageTask::closeHandles() {
    closeRead();
    if (mTimer) {
        uv_timer_stop(&mTimer->handle);
        uv_close(reinterpret_cast<uv_handle_t*>(&mTimer->handle), [](uv_handle_t* handle) {
            delete static_cast<TimerContext*>(handle->data);
        });
        mTimer = nullptr;
    }
}

void PackageTask::closeRead() {
    if (mPoll) {
        uv_poll_stop(&mPoll->handle);
        uv_close(reinterpret_cast<uv_handle_t*>(&mPoll->handle), [](uv_handle_t* handle) {
            delete static_cast<PollContext*>(handle->data);
        });
        mPoll = nullptr;
    }
    if (mReadFd >= 0) {
        close(mReadFd);
        mReadFd = -1;
    }
    mPipeEnded = true;
}

void PackageTask::drain() {
    // A noisy worker cannot monopolize the service event loop.
    for (unsigned count = 0; count < 64 && mReadFd >= 0 && !mFinished; ++count) {
        uint8_t progress;
        ssize_t nread = read(mReadFd, &progress, sizeof(progress));
        if (nread == sizeof(progress)) {
            if (mTerminal >= 0) {
                if (progress != mTerminal) {
                    mError = -EPROTO;
                    mNeedTerminate = true;
                }
            } else if (progress >= 100) {
                mTerminal = progress;
                if (progress > 100) {
                    mError = -EIO;
                    mNeedTerminate = true;
                }
                else {
                    auto callback = mProgress;
                    if (callback) callback(progress);
                }
            } else {
                auto callback = mProgress;
                if (callback) callback(progress);
            }
        } else if (nread == 0) {
            if (mTerminal < 0 && mError == 0) mError = -EPIPE;
            if (mError != 0) mNeedTerminate = true;
            closeRead();
        } else if (errno == EINTR) {
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        } else {
            mError = -errno;
            mNeedTerminate = true;
            closeRead();
        }
    }
}

void PackageTask::onPoll(int status, int events) {
    if (mFinished) return;
    if (status < 0) {
        mError = status;
        mNeedTerminate = true;
        closeRead();
    } else if (events & (UV_READABLE | UV_DISCONNECT)) {
        drain();
    }
    tick();
}

void PackageTask::tick() {
    if (mFinished || !mStarted) return;
    drain();
    if (!mExited && mPid > 0) {
        pid_t pid = waitpid(mPid, &mExitStatus, WNOHANG);
        if (pid == mPid) {
            mExited = true;
            mPid = -1;
            if (!WIFEXITED(mExitStatus) || WEXITSTATUS(mExitStatus) != 0) {
                if (mError == 0) mError = -EIO;
            }
        } else if (pid < 0 && errno != EINTR) {
            // Retained child status is required. ECHILD is an error rather
            // than evidence that a success byte implies a clean child exit.
            mError = -errno;
            if (errno == ECHILD) {
                mExited = true;
                mPid = -1;
                closeRead();
            }
        }
    }

    const uint64_t now = uv_now(mLoop);
    if (now - mStartedAt >= CONFIG_SYSTEM_PACKAGE_SERVICE_TASK_TIMEOUT_MS) {
        if (mError == 0) mError = -ETIMEDOUT;
        mNeedTerminate = true;
        closeRead();
    }
    if (!mExited && mNeedTerminate) {
        if (!mSentTerm) {
            kill(mPid, SIGTERM);
            mSentTerm = true;
            mTerminatedAt = now;
        } else if (!mSentKill && now - mTerminatedAt >= 1000) {
            // A temporarily full signal queue must not permanently disable
            // termination. Retain the stage and retry at a bounded rate.
            mSentKill = kill(mPid, SIGKILL) == 0 || errno == ESRCH;
            if (!mSentKill) mTerminatedAt = now;
        }
    }
    if (mExited && mPipeEnded) {
        int result = mError;
        if (result == 0 && mTerminal != 100) result = -EPROTO;
        finish(result, true);
    }
}

void PackageTask::cancel() {
    if (mFinished) return;
    if (!mStarted) { finish(-ECANCELED, false); return; }
    mError = -ECANCELED;
    mNeedTerminate = true;
    closeRead();
    tick();
}

void PackageTask::finish(int result, bool notify) {
    if (mFinished) return;
    mFinished = true;
    closeHandles();
    if (mReadFd >= 0) close(mReadFd);
    if (mWriteFd >= 0) close(mWriteFd);
    mReadFd = mWriteFd = -1;
    auto completion = std::move(mCompletion);
    mProgress = nullptr;
    if (notify && completion) completion(result);
    mLifetime.reset();
}

} // namespace pm
} // namespace os
