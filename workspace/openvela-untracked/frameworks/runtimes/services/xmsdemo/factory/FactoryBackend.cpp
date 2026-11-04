// SPDX-License-Identifier: Apache-2.0
// Native openvela implementation of the Espressif Korvo factory-demo behavior.
// Button colors/order follow Espressif factory_demo main.c (Apache-2.0,
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD).
#include <nuttx/config.h>
#include "FactoryBackend.h"
#include <nuttx/input/buttons.h>
#include <nuttx/input/mouse.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/videoio.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <time.h>
#ifdef CONFIG_MEDIA
#include <media_player.h>
#include <media_recorder.h>
#endif

// Enable only with the inode-owned mouse upper half and USB producer teardown.
// The factory build includes those lifetime fixes; physical input and hotplug
// acceptance remain separate from host regression and compilation.
#ifndef FACTORY_BACKEND_MOUSE_HOTPLUG_SAFE
#define FACTORY_BACKEND_MOUSE_HOTPLUG_SAFE 0
#endif

namespace os::app::factory {
namespace {
constexpr unsigned kPreview = 240;
constexpr size_t kPreviewBytes = kPreview * kPreview * 2;
constexpr size_t kCaptureBytes = 640 * 480 * 2;
constexpr size_t kPcmRate = 16000 * 2 * 2; // native ES8389: 16 kHz, stereo, s16le
constexpr size_t kPcmCapacity = kPcmRate * 6;
constexpr uint32_t kColors[4] = {0x001c1204, 0x00060e1c, 0x00041c0a, 0x001c0404};
constexpr unsigned kButtonBits[4] = {1, 0, 2, 3}; // SET, MODE, VOL-, VOL+
constexpr const char* kSdRoot = "/mnt/sdcard";
class Lock {
public:
    explicit Lock(pthread_mutex_t& mutex) : mMutex(mutex) { pthread_mutex_lock(&mMutex); }
    ~Lock() { pthread_mutex_unlock(&mMutex); }
private:
    pthread_mutex_t& mMutex;
};
uint64_t nowNs()
{
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) return 0;
    return uint64_t(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
}
void status(Status& out, State state, int error, const char* text)
{
    out.state = state;
    out.error = error;
    std::snprintf(out.text, sizeof(out.text), "%s", text);
}
int ctl(int fd, int command, void* argument)
{
    int ret;
    do { ret = ioctl(fd, command, reinterpret_cast<unsigned long>(argument)); }
    while (ret < 0 && errno == EINTR);
    return ret < 0 ? -errno : 0;
}
void delay(unsigned ms) { poll(nullptr, 0, ms); }
struct Work { Command command; int argument; };

class NativeBackend final : public FactoryBackend {
public:
    ~NativeBackend() override { shutdown(); pthread_mutex_destroy(&mMutex); }
    int start() override;
    int setActive(bool active) override;
    int setCameraPreview(bool enabled) override;
    int submit(Command command, int argument) override;
    int snapshot(Snapshot& output) override;
    int acquireFrame(CameraFrame& output) override;
    void releaseFrame(uintptr_t lease) override { mReleased.store(lease, std::memory_order_release); }
    void shutdown() override;
private:
    static void* cameraEntry(void* p) { static_cast<NativeBackend*>(p)->cameraRun(); return nullptr; }
    static void* audioEntry(void* p) { static_cast<NativeBackend*>(p)->audioRun(); return nullptr; }
    static void* peripheralEntry(void* p) { static_cast<NativeBackend*>(p)->peripheralRun(); return nullptr; }
    void cameraRun();
    void visionRun();
    int cameraSession();
    int queueCapture(int fd, unsigned index, uint8_t* memory);
    bool cameraWanted();
    void audioRun();
    void peripheralRun();
    void ledWrite(int index);
    void storageProbe();
    void consumeRelease(); // mutex held
    bool cancelled() const { return mStop.load(std::memory_order_acquire); }
    void cameraStatus(int error, const char* text);
#ifdef CONFIG_MEDIA
    struct AudioIo { NativeBackend* self; void* handle; bool recording; size_t size; };
    static void audioEvent(void* cookie, int event, int result, const char*);
    static void* audioIoEntry(void* cookie);
    void audioSession(bool recording);
#endif
    pthread_mutex_t mMutex = PTHREAD_MUTEX_INITIALIZER;
    std::atomic<bool> mStop{false};
    std::atomic<uintptr_t> mReleased{0};
    bool mStarted = false, mActive = false, mPreview = true, mDigitRequested = false;
    pthread_t mThreads[3]{};
    unsigned mThreadCount = 0;
    Snapshot mSnapshot;
    Work mQueue[8]{};
    unsigned mHead = 0, mCount = 0;
    int mAudioRequest = -1;
    bool mAudioBusy = false, mAudioCancel = false;
    bool mAudioStarted = false, mAudioCompleted = false, mAudioIoDone = false;
    bool mRecorderClosing = false, mRecorderCloseEpipe = false;
    int mAudioError = 0;
    uint8_t* mPcm = nullptr;
    size_t mRecorded = 0;
    uint8_t* mFrames[2]{};
    int mLatest = -1, mLeasedIndex = -1;
    uintptr_t mLease = 0, mNextLease = 0;
    uint64_t mFrameTime[2]{}, mFrameSequence[2]{}, mAcquiredSequence = 0;
};

int NativeBackend::start()
{
    if (pthread_mutex_trylock(&mMutex)) return -EAGAIN;
    if (mStarted || cancelled()) { pthread_mutex_unlock(&mMutex); return -EALREADY; }
    mStarted = true;
    mSnapshot.audioCapacityBytes = kPcmCapacity;
    status(mSnapshot.camera, State::Starting, 0, "Waiting for capture device");
    status(mSnapshot.audio, State::Idle, 0, "MEDIA session not opened");
    mSnapshot.audioState = AudioState::Idle;
    status(mSnapshot.storage, State::Idle, 0, "SD probe not run");
    status(mSnapshot.buttons, State::Starting, 0, "Opening native buttons");
    status(mSnapshot.led, State::Idle, 0, "LED not written");
    status(mSnapshot.usb, State::Starting, 0, "Waiting for native HID nodes");
    void* (*entries[3])(void*) = {cameraEntry, audioEntry, peripheralEntry};
    pthread_attr_t attr;
    int ret = pthread_attr_init(&attr);
    if (!ret) {
        ret = pthread_attr_setstacksize(&attr, 8192);
        for (unsigned i = 0; !ret && i < 3; ++i) {
            ret = pthread_create(&mThreads[i], &attr, entries[i], this);
            if (!ret) ++mThreadCount;
        }
        pthread_attr_destroy(&attr);
    }
    if (ret) mStop.store(true, std::memory_order_release);
    pthread_mutex_unlock(&mMutex);
    return -ret; // On partial startup, shutdown joins all successfully created threads.
}
int NativeBackend::setActive(bool active)
{
    if (pthread_mutex_trylock(&mMutex)) return -EAGAIN;
    int ret = !mStarted || cancelled() ? -ESHUTDOWN : 0;
    if (!ret) {
        mActive = active;
        if (!active) {
            mAudioCancel = true;
            mDigitRequested = false;
            mCount = 0;
            mHead = 0;
        }
    }
    pthread_mutex_unlock(&mMutex);
    return ret;
}
int NativeBackend::setCameraPreview(bool enabled)
{
    if (pthread_mutex_trylock(&mMutex)) return -EAGAIN;
    int ret = !mStarted || cancelled() ? -ESHUTDOWN : 0;
    if (!ret) mPreview = enabled;
    pthread_mutex_unlock(&mMutex);
    return ret;
}
int NativeBackend::submit(Command command, int argument)
{
    if (pthread_mutex_trylock(&mMutex)) return -EAGAIN;
    int ret = 0;
    if (!mStarted || cancelled()) ret = -ESHUTDOWN;
    else if (!mActive) ret = -EHOSTDOWN;
    else if (command == Command::StartDigit || command == Command::StopDigit) {
#ifdef CONFIG_ESP32S31_VISION
        if (mAudioBusy) ret = -EBUSY;
        else mDigitRequested = command == Command::StartDigit;
#else
        ret = -ENOSYS;
#endif
    }
    else if (command == Command::StopAudio || command == Command::ResetAudio) {
#ifndef CONFIG_MEDIA
        ret = -ENOSYS;
#else
        mAudioCancel = true;
        if (mAudioBusy) mSnapshot.audioState = AudioState::Stopping;
        if (command == Command::ResetAudio && !mAudioBusy) {
            mRecorded = 0;
            mSnapshot.recordedBytes = 0;
            mSnapshot.hasRecording = false;
            mSnapshot.audioState = AudioState::Idle;
            status(mSnapshot.audio, State::Idle, 0, "Audio reset");
        }
#endif
    } else if (command == Command::StartRecording || command == Command::PlayRecording) {
        if (mAudioBusy) ret = -EBUSY;
        else if (command == Command::PlayRecording && !mSnapshot.hasRecording) ret = -ENODATA;
#ifndef CONFIG_MEDIA
        else ret = -ENOSYS;
#else
        else {
            mAudioRequest = command == Command::StartRecording ? 1 : 0;
            mAudioBusy = true;
            mAudioCancel = false;
            status(mSnapshot.audio, State::Starting, 0, "Opening MEDIA session");
        }
#endif
    } else if (command != Command::SelectLed && command != Command::ProbeStorage) ret = -EINVAL;
    else if (command == Command::SelectLed && (argument < 0 || argument > 3)) ret = -EINVAL;
    else if (mCount == 8) ret = -EAGAIN;
    else { mQueue[(mHead + mCount) % 8] = {command, argument}; ++mCount; }
    pthread_mutex_unlock(&mMutex);
    return ret;
}
int NativeBackend::snapshot(Snapshot& output)
{
    if (pthread_mutex_trylock(&mMutex)) return -EAGAIN;
    output = mSnapshot;
    output.sampledNs = nowNs();
    pthread_mutex_unlock(&mMutex);
    return 0;
}
void NativeBackend::consumeRelease()
{
    uintptr_t token = mReleased.exchange(0, std::memory_order_acq_rel);
    if (token && token == mLease) { mLease = 0; mLeasedIndex = -1; }
}
int NativeBackend::acquireFrame(CameraFrame& output)
{
    if (pthread_mutex_trylock(&mMutex)) return -EAGAIN;
    consumeRelease();
    int ret = -EAGAIN;
    if (mLease) ret = -EBUSY;
    else if (mActive && mPreview && !cancelled() && mLatest >= 0 && mFrameSequence[mLatest] > mAcquiredSequence) {
        mLeasedIndex = mLatest;
        do { ++mNextLease; } while (!mNextLease);
        mLease = mNextLease;
        output = {mFrames[mLatest], kPreviewBytes, kPreview * 2,
                  kPreview, kPreview, mFrameTime[mLatest], mFrameSequence[mLatest], mLease};
        mAcquiredSequence = output.sequence;
        ret = 0;
    }
    pthread_mutex_unlock(&mMutex);
    return ret;
}
void NativeBackend::shutdown()
{
    // Serialized with start/destruction by the Activity lifecycle owner.
    mStop.store(true, std::memory_order_release);
    for (unsigned i = 0; i < mThreadCount; ++i) pthread_join(mThreads[i], nullptr);
    mThreadCount = 0;
    // Caller releases its frame lease before shutdown; buffers survive until join.
    std::free(mFrames[0]); std::free(mFrames[1]); std::free(mPcm);
    mFrames[0] = mFrames[1] = mPcm = nullptr;
    Lock lock(mMutex);
    mLatest = mLeasedIndex = -1;
    mLease = 0;
    mCount = 0;
}
bool NativeBackend::cameraWanted()
{
    Lock lock(mMutex);
    return !cancelled() && mActive && mPreview;
}
void NativeBackend::cameraStatus(int error, const char* text)
{
    Lock lock(mMutex);
    status(mSnapshot.camera, error ? (error == -ENOENT || error == -ENODEV || error == -ENOSYS ?
           State::Unavailable : State::Error) : State::Idle, error, text);
}
int NativeBackend::queueCapture(int fd, unsigned index, uint8_t* memory)
{
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_USERPTR;
    buf.index = index;
    buf.m.userptr = reinterpret_cast<unsigned long>(memory);
    buf.length = kCaptureBytes;
    return ctl(fd, VIDIOC_QBUF, &buf);
}
int NativeBackend::cameraSession()
{
    int fd = open("/dev/video0", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -errno;
    uint8_t* buffers[2]{};
    v4l2_format format{};
    v4l2_requestbuffers req{};
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bool streamAttempted = false;
    int ret = 0;
    format.type = type;
    format.fmt.pix.width = 640;
    format.fmt.pix.height = 480;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    format.fmt.pix.field = V4L2_FIELD_ANY;
    ret = ctl(fd, VIDIOC_S_FMT, &format);
    if (!ret) ret = ctl(fd, VIDIOC_G_FMT, &format);
    // Current S31 bridge is fixed packed RGB565. Reject unknown layouts instead
    // of interpreting padding, another resolution, or a different byte order.
    if (!ret && (format.fmt.pix.width != 640 || format.fmt.pix.height != 480 ||
        format.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565 ||
        (format.fmt.pix.bytesperline && format.fmt.pix.bytesperline != 1280) ||
        (format.fmt.pix.sizeimage && format.fmt.pix.sizeimage != kCaptureBytes) ||
        (format.fmt.pix.field != V4L2_FIELD_ANY && format.fmt.pix.field != V4L2_FIELD_NONE))) ret = -ENOTSUP;
    req.type = type; req.memory = V4L2_MEMORY_USERPTR;
    req.mode = V4L2_BUF_MODE_FIFO; req.count = 2;
    if (!ret) ret = ctl(fd, VIDIOC_REQBUFS, &req);
    if (!ret && req.count != 2) ret = -ENOBUFS;
    for (unsigned i = 0; !ret && i < 2; ++i) {
        buffers[i] = static_cast<uint8_t*>(std::malloc(kCaptureBytes));
        ret = buffers[i] ? queueCapture(fd, i, buffers[i]) : -ENOMEM;
    }
    if (!ret && cameraWanted()) { streamAttempted = true; ret = ctl(fd, VIDIOC_STREAMON, &type); }
    uint64_t last = nowNs();
    while (!ret && streamAttempted && cameraWanted()) {
        if (nowNs() - last > 3000000000ULL) { ret = -ETIMEDOUT; break; }
        pollfd pfd{fd, POLLIN, 0};
        int ready = poll(&pfd, 1, 25);
        if (ready < 0) { if (errno == EINTR) continue; ret = -errno; break; }
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) { ret = -EIO; break; }
        if (!(pfd.revents & POLLIN)) {
            if (nowNs() - last > 3000000000ULL) ret = -ETIMEDOUT;
            continue;
        }
        v4l2_buffer frame{};
        frame.type = type; frame.memory = V4L2_MEMORY_USERPTR;
        ret = ctl(fd, VIDIOC_DQBUF, &frame);
        if (ret == -EAGAIN) { ret = 0; continue; }
        if (ret) break;
        if (frame.index >= 2 || frame.type != type || frame.memory != V4L2_MEMORY_USERPTR ||
            frame.m.userptr != reinterpret_cast<unsigned long>(buffers[frame.index]) ||
            frame.length != kCaptureBytes || frame.bytesused != kCaptureBytes ||
            (frame.flags & V4L2_BUF_FLAG_ERROR) || frame.timestamp.tv_sec < 0 ||
            frame.timestamp.tv_usec < 0 || frame.timestamp.tv_usec >= 1000000) { ret = -EPROTO; break; }
        last = nowNs();
        // S31 DVP uses clock_systime_timespec at deferred-copy completion.
        uint64_t captureNs = uint64_t(frame.timestamp.tv_sec) * 1000000000ULL +
                             uint64_t(frame.timestamp.tv_usec) * 1000;
        int slot;
        uint64_t sequence;
        {
            Lock lock(mMutex);
            consumeRelease();
            slot = mLeasedIndex == 0 ? 1 : 0;
            if (mLatest == slot) {
                if (mFrameSequence[slot] > mAcquiredSequence) ++mSnapshot.cameraDroppedFrames;
                mLatest = -1; // keep unpublished memory out of acquireFrame
            }
            sequence = ++mSnapshot.capturedFrames;
            mSnapshot.cameraWidth = 640; mSnapshot.cameraHeight = 480;
        }
        // Central square, exact 2:1 sampling. Input RGB565 is native-endian.
        const uint8_t* src = buffers[frame.index];
        for (unsigned y = 0; y < kPreview; ++y)
            for (unsigned x = 0; x < kPreview; ++x)
                std::memcpy(mFrames[slot] + (y * kPreview + x) * 2,
                            src + (y * 2 * 640 + 80 + x * 2) * 2, 2);
        {
            Lock lock(mMutex);
            if (mLatest >= 0 && mFrameSequence[mLatest] > mAcquiredSequence)
                ++mSnapshot.cameraDroppedFrames;
            mFrameTime[slot] = captureNs;
            mFrameSequence[slot] = sequence;
            mLatest = slot;
            status(mSnapshot.camera, State::Ready, 0, "RGB565 capture completed");
        }
        ret = queueCapture(fd, frame.index, buffers[frame.index]);
    }
    // close drains deferred USERPTR copies. Never free USERPTR memory first.
    if (streamAttempted) {
        int stopped = ctl(fd, VIDIOC_STREAMOFF, &type);
        if (!ret) ret = stopped;
    }
    int closed = close(fd);
    if (!ret && closed < 0) ret = -errno;
    for (auto* buffer : buffers) std::free(buffer);
    return ret;
}
void NativeBackend::cameraRun()
{
#ifdef CONFIG_ESP32S31_VISION
    visionRun();
    return;
#endif
    // The maintained driver cannot be enabled on the current SMP kernel profile.
    // Keep the capture implementation compiled/typechecked, but do not probe a
    // foreign /dev/video0 when this specific bridge is absent from the profile.
#ifndef CONFIG_ESP32S31_CAMERA_V4L2
    cameraStatus(-ENOSYS, "Profile lacks S31 V4L2 capture");
    return;
#else
    mFrames[0] = static_cast<uint8_t*>(std::malloc(kPreviewBytes));
    mFrames[1] = static_cast<uint8_t*>(std::malloc(kPreviewBytes));
    if (!mFrames[0] || !mFrames[1]) { cameraStatus(-ENOMEM, "Preview allocation failed"); return; }
    bool attempted = false;
    while (!cancelled()) {
        if (!cameraWanted()) { attempted = false; delay(25); continue; }
        if (attempted) { delay(25); continue; } // retry on explicit off/on, not a failure loop
        attempted = true;
        cameraStatus(0, "Starting camera capture");
        int ret = cameraSession();
        cameraStatus(ret, ret ? "Camera capture failed; toggle to retry" : "Camera stopped");
    }
#endif
}

void NativeBackend::visionRun()
{
#ifdef CONFIG_ESP32S31_VISION
    mFrames[0] = static_cast<uint8_t*>(std::malloc(kPreviewBytes));
    mFrames[1] = static_cast<uint8_t*>(std::malloc(kPreviewBytes));
    int control = open("/dev/vision", O_RDWR);
    int frame = open("/dev/vision-frame", O_RDONLY | O_NONBLOCK);
    if (!mFrames[0] || !mFrames[1] || control < 0 || frame < 0) {
        int error = !mFrames[0] || !mFrames[1] ? -ENOMEM : -errno;
        cameraStatus(error, "Camera service unavailable");
        { Lock lock(mMutex); status(mSnapshot.digit, State::Error, error, "Vision service unavailable"); }
        if (frame >= 0) close(frame);
        if (control >= 0) close(control);
        return;
    }
    int mode = 0, attempted = -1;
    uint64_t sequence = 0, lastFrame = nowNs(), lastStatus = 0;
    while (!cancelled()) {
        int desired;
        { Lock lock(mMutex); desired = !mActive ? 0 : mDigitRequested ? 2 : mPreview ? 1 : 0; }
        if (desired != attempted) {
            attempted = desired;
            { Lock lock(mMutex);
              status(mSnapshot.digit, State::Starting, 0, desired == 2 ? "Starting original digit demo" : "Returning to Factory"); }
            const char* command = desired == 2 ? "start" : desired == 1 ? "preview" : "stop";
            int ret = write(control, command, std::strlen(command));
            int error = ret < 0 ? -errno : 0;
            // If startup failed, stop must acknowledge retirement before preview
            // may be restored. The driver retains an uncertain lease on failure.
            int cleanup = 0;
            if (error) {
                cleanup = write(control, "stop", 4);
                mode = -1;
            } else mode = desired;
            lastFrame = nowNs();
            { Lock lock(mMutex);
              mSnapshot.digitDisplayOwned = error ? cleanup < 0 : desired == 2;
              status(mSnapshot.digit, error ? State::Error : desired == 2 ? State::Ready : State::Idle,
                     error, error ? "Vision transition failed" : desired == 2 ? "Original CPU1 SIMD demo" : "Factory");
              status(mSnapshot.camera, error ? State::Error : desired == 2 ? State::Busy : State::Idle,
                     error, desired == 2 ? "Original VGA recognition" : "Native preview"); }
            std::printf("S31Factory vision mode=%d requested=%d error=%d\n", mode, desired, error);
        }
        if (mode == 1) {
            int index;
            { Lock lock(mMutex);
              consumeRelease();
              index = mLeasedIndex == 0 ? 1 : 0;
              if (mLatest == index) mLatest = -1;
            }
            ssize_t count = read(frame, mFrames[index], kPreviewBytes);
            if (count == static_cast<ssize_t>(kPreviewBytes)) {
                lastFrame = nowNs();
                Lock lock(mMutex);
                mFrameTime[index] = lastFrame; mFrameSequence[index] = ++sequence;
                mLatest = index; mSnapshot.capturedFrames = sequence;
                mSnapshot.cameraWidth = 640; mSnapshot.cameraHeight = 480;
                status(mSnapshot.camera, State::Ready, 0, "Native RGB565 preview");
            } else if (count < 0 && errno != EAGAIN) {
                cameraStatus(-errno, "Preview read failed");
            } else if (nowNs() - lastFrame > 4000000000ULL) {
                cameraStatus(-ETIMEDOUT, "No camera frame");
            }
        }
        if (mode > 0 && nowNs() - lastStatus >= 1000000000ULL) {
            lastStatus = nowNs();
            int state = open("/dev/vision", O_RDONLY);
            if (state >= 0) {
                char info[256]{};
                ssize_t n = read(state, info, sizeof(info)-1);
                close(state);
                if (n > 0 && (std::strstr(info, "state=fault") || std::strstr(info, "state=stopped"))) {
                    mode = -1;
                    Lock lock(mMutex);
                    status(mSnapshot.digit, State::Error, -EIO, "Vision stopped; return to Factory");
                }
            }
        }
        delay(20);
    }
    write(control, "stop", 4);
    close(frame);
    close(control); // Driver also performs cooperative stop on owner loss.
#endif
}

#ifdef CONFIG_MEDIA
void NativeBackend::audioEvent(void* cookie, int event, int result, const char*)
{
    auto& self = *static_cast<NativeBackend*>(cookie);
    Lock lock(self.mMutex);
    // Closing the recorder's buffer socket can complete its writer with EPIPE.
    // Preserve errors received before teardown and all other failure types.
    if (self.mRecorderClosing && event == MEDIA_EVENT_COMPLETED && result == -EPIPE)
        self.mRecorderCloseEpipe = true;
    else if (result < 0 && !self.mAudioError)
        self.mAudioError = result;
    if (event == MEDIA_EVENT_STARTED && !result) self.mAudioStarted = true;
    if (event == MEDIA_EVENT_COMPLETED && !result) self.mAudioCompleted = true;
    // No MEDIA lifecycle call and no UI pointer is used by the listener.
}
void* NativeBackend::audioIoEntry(void* cookie)
{
    auto& io = *static_cast<AudioIo*>(cookie);
    auto& self = *io.self;
    size_t count = 0;
    int error = 0;
    while (count < io.size) {
        {
            Lock lock(self.mMutex);
            if (self.cancelled() || self.mAudioCancel || self.mAudioError) break;
        }
        size_t chunk = io.size - count;
        if (chunk > 4096) chunk = 4096;
        // Use MEDIA-owned I/O, not its borrowed fd: close_socket cancels these
        // waits while the handle and stack cookie stay alive through join.
        ssize_t n = io.recording ? media_recorder_read_data(io.handle, self.mPcm + count, chunk)
                                : media_player_write_data(io.handle, self.mPcm + count, chunk);
        if (n == -EINTR) continue;
        if (n <= 0) { error = n < 0 ? int(n) : -EIO; break; }
        if (size_t(n) > chunk) { error = -EPROTO; break; }
        count += size_t(n);
        if (io.recording) {
            Lock lock(self.mMutex);
            self.mRecorded = count - count % 4;
            self.mSnapshot.recordedBytes = self.mRecorded;
            self.mSnapshot.audioPositionMs = self.mRecorded * 1000 / kPcmRate;
        }
    }
    if (!io.recording && count == io.size) media_player_close_socket(io.handle);
    {
        Lock lock(self.mMutex);
        if (error && !self.mAudioCancel && !self.cancelled() && !self.mAudioError)
            self.mAudioError = error;
        self.mAudioIoDone = true;
    }
    return nullptr;
}
void NativeBackend::audioSession(bool recording)
{
    {
        Lock lock(mMutex);
        mAudioError = 0;
        mAudioStarted = mAudioCompleted = mAudioIoDone = false;
        mRecorderClosing = mRecorderCloseEpipe = false;
        if (recording) {
            mRecorded = 0; mSnapshot.recordedBytes = 0; mSnapshot.hasRecording = false;
        }
        mSnapshot.audioPositionMs = 0;
    }
    if (!mPcm) mPcm = static_cast<uint8_t*>(std::malloc(kPcmCapacity));
    int ret = mPcm ? 0 : -ENOMEM;
    auto abortRequested = [this]() {
        Lock lock(mMutex);
        return cancelled() || mAudioCancel || !mActive;
    };
    void* handle = nullptr;
    if (!ret && !abortRequested()) {
        handle = recording ? media_recorder_open(MEDIA_SOURCE_MIC) : media_player_open(MEDIA_STREAM_MUSIC);
        if (!handle) ret = -ENODEV;
    }
    if (handle && !abortRequested())
        ret = recording ? media_recorder_set_event_callback(handle, this, audioEvent)
                        : media_player_set_event_callback(handle, this, audioEvent);
    // Fixed stream gain preserves upstream volume 55 without modifying the
    // device-wide policy of another application. Physical hearing is deferred.
    if (!ret && handle && !recording && !abortRequested()) ret = media_player_set_volume(handle, 0.55f);
    if (!ret && handle && !abortRequested()) {
        ret = recording ? media_recorder_prepare(handle, nullptr,
            "format=s16le:audio_codec=pcm_s16le:sample_rate=16000:ch_layout=stereo")
            : media_player_prepare(handle, nullptr, "format=s16le:sample_rate=16000:ch_layout=stereo");
    }
    if (!ret && handle && !abortRequested())
        ret = recording ? media_recorder_start(handle) : media_player_start(handle);
    AudioIo io{this, handle, recording, recording ? kPcmCapacity : mRecorded};
    pthread_t dataThread{};
    bool dataCreated = false;
    if (!ret && handle && !abortRequested()) {
        pthread_attr_t attr;
        int created = pthread_attr_init(&attr);
        if (!created) {
            created = pthread_attr_setstacksize(&attr, 8192);
            if (!created) created = pthread_create(&dataThread, &attr, audioIoEntry, &io);
            pthread_attr_destroy(&attr);
        }
        if (created) ret = -created;
        else dataCreated = true;
    }
    uint64_t begin = nowNs(), nextPosition = begin;
    while (!ret && dataCreated && !abortRequested()) {
        bool done, started;
        {
            Lock lock(mMutex);
            ret = mAudioError;
            started = mAudioStarted;
            done = recording ? mAudioIoDone : mAudioCompleted;
            if (!ret && mAudioStarted) {
                mSnapshot.audioState = recording ? AudioState::Recording : AudioState::Playing;
                status(mSnapshot.audio, State::Busy, 0, recording ? "Recording microphone PCM" : "MEDIA playback running");
            }
        }
        if (ret || done) break;
        uint64_t now = nowNs();
        if (now - begin > 20000000000ULL) { ret = -ETIMEDOUT; break; }
        if (!recording && started && now >= nextPosition) {
            unsigned position = 0;
            int queried = media_player_get_position(handle, &position);
            if (!queried) { Lock lock(mMutex); mSnapshot.audioPositionMs = position; }
            else { ret = queried; break; }
            nextPosition = now + 250000000ULL;
        }
        delay(20);
    }
    {
        Lock lock(mMutex);
        mSnapshot.audioState = AudioState::Stopping;
        status(mSnapshot.audio, State::Busy, 0, "Closing MEDIA session");
        mRecorderClosing = recording;
    }
    if (handle) {
        // Cancel data first, join it before stop/close, then join the listener by
        // synchronous close. Never pthread_cancel through MEDIA or destroy an
        // object whose API call/callback may still reference it.
        if (recording) media_recorder_close_socket(handle);
        else media_player_close_socket(handle);
        if (dataCreated) pthread_join(dataThread, nullptr);
        int stopped = recording ? media_recorder_stop(handle) : media_player_stop(handle);
        if (!ret && stopped < 0) ret = stopped;
        int closed = recording ? media_recorder_close(handle) : media_player_close(handle, 0);
        if (!ret && closed < 0) ret = closed;
    }
    {
        Lock lock(mMutex);
        if (mRecorderCloseEpipe)
            std::printf("S31Factory recorder socket closed: completion EPIPE during teardown\n");
        if (!ret && mAudioError) ret = mAudioError;
        bool complete = !recording && mAudioCompleted;
        bool stopped = mAudioCancel || cancelled() || !mActive;
        if (!ret && recording && !mRecorded && !stopped) ret = -ENODATA;
        mSnapshot.hasRecording = recording ? !ret && mRecorded != 0 : mSnapshot.hasRecording;
        mSnapshot.audioState = ret ? AudioState::Error : AudioState::Idle;
        status(mSnapshot.audio, ret ? State::Error : State::Ready, ret,
               ret ? "MEDIA operation failed" : recording ?
               (mRecorded ? "Recording retained in RAM" : "Recording canceled") :
               complete ? "Playback completed; hearing unverified" : "Playback stopped");
        mAudioBusy = false;
    }
}
#endif
void NativeBackend::audioRun()
{
#ifndef CONFIG_MEDIA
    Lock lock(mMutex);
    mSnapshot.audioState = AudioState::Unavailable;
    status(mSnapshot.audio, State::Unavailable, -ENOSYS, "Profile lacks MEDIA");
#else
    while (!cancelled()) {
        int request = -1;
        {
            Lock lock(mMutex);
            request = mAudioRequest;
            mAudioRequest = -1;
        }
        if (request < 0) delay(20);
        else audioSession(request == 1);
    }
#endif
}

void NativeBackend::ledWrite(int index)
{
    int ret = 0;
    int fd = open("/dev/leds0", O_WRONLY | O_CLOEXEC);
    if (fd < 0) ret = -errno;
    else {
        // This native driver consumes one uint32_t 0x00RRGGBB per LED.
        uint32_t color = kColors[index];
        ssize_t n;
        do { n = write(fd, &color, sizeof(color)); } while (n < 0 && errno == EINTR);
        if (n != sizeof(color)) ret = n < 0 ? -errno : -EIO;
        if (close(fd) < 0 && !ret) ret = -errno;
    }
    Lock lock(mMutex);
    status(mSnapshot.led, ret ? State::Unavailable : State::Ready, ret,
           ret ? "LED device write failed" : "LED write completed");
    if (!ret) {
        mSnapshot.activeLedButton = index;
        mSnapshot.ledRed = kColors[index] >> 16;
        mSnapshot.ledGreen = kColors[index] >> 8;
        mSnapshot.ledBlue = kColors[index];
    }
}
void NativeBackend::storageProbe()
{
    {
        Lock lock(mMutex);
        status(mSnapshot.storage, State::Busy, 0, "Verifying own SD probe file");
        mSnapshot.sdProbePassed = false;
    }
    struct statfs fs{};
    int ret = statfs(kSdRoot, &fs) < 0 ? -errno : 0;
    if (!ret && fs.f_type != MSDOS_SUPER_MAGIC) ret = -ENODEV;
    {
        Lock lock(mMutex);
        mSnapshot.sdMounted = !ret;
        mSnapshot.sdFreeBytes = ret ? 0 : uint64_t(fs.f_bavail) * fs.f_bsize;
    }
    char path[] = "/mnt/sdcard/.factory-probe-XXXXXX";
    int fd = -1;
    bool owned = false;
    if (!ret && !cancelled()) {
        fd = mkstemp(path); // atomic O_CREAT|O_EXCL, never adopts an existing file
        if (fd < 0) ret = -errno;
        else owned = true;
    } else if (!ret) ret = -ECANCELED;
    uint8_t sent[512], received[512];
    uint64_t nonce = nowNs();
    for (unsigned i = 0; i < sizeof(sent); ++i)
        sent[i] = uint8_t(i * 73u + (nonce >> ((i % 8) * 8)));
    size_t offset = 0;
    while (!ret && offset < sizeof(sent)) {
        if (cancelled()) { ret = -ECANCELED; break; }
        ssize_t n = write(fd, sent + offset, sizeof(sent) - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) ret = n < 0 ? -errno : -EIO;
        else offset += n;
    }
    if (!ret && fsync(fd) < 0) ret = -errno;
    if (!ret && lseek(fd, 0, SEEK_SET) != 0) ret = errno ? -errno : -EIO;
    offset = 0;
    while (!ret && offset < sizeof(received)) {
        ssize_t n = read(fd, received + offset, sizeof(received) - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) ret = n < 0 ? -errno : -EIO;
        else offset += n;
    }
    if (!ret && std::memcmp(sent, received, sizeof(sent))) ret = -EILSEQ;
    uint8_t extra;
    if (!ret) {
        ssize_t tail;
        do { tail = read(fd, &extra, 1); } while (tail < 0 && errno == EINTR);
        if (tail != 0) ret = tail < 0 ? -errno : -EIO;
    }
    if (fd >= 0 && close(fd) < 0 && !ret) ret = -errno;
    // Remove only the file created by this invocation; failure remains visible.
    if (owned && unlink(path) < 0 && !ret) ret = -errno;
    Lock lock(mMutex);
    ++mSnapshot.sdProbeCount;
    mSnapshot.sdProbePassed = !ret;
    status(mSnapshot.storage, ret ? State::Error : State::Ready, ret,
           ret ? "SD probe or cleanup failed" : "Own SD file readback verified and removed");
}

void NativeBackend::peripheralRun()
{
    int buttonFd = -1, hidFd[2] = {-1, -1};
    bool wasActive = false;
    bool hidSeen[2] = {false, false};
    uint64_t nextOpen = 0;
    while (!cancelled()) {
        bool active;
        Work work{};
        bool haveWork = false;
        {
            Lock lock(mMutex);
            active = mActive;
            if (active && mCount) {
                work = mQueue[mHead]; mHead = (mHead + 1) % 8; --mCount;
                haveWork = true;
            }
        }
        if (!active) {
            if (buttonFd >= 0) { close(buttonFd); buttonFd = -1; }
            for (auto& fd : hidFd) if (fd >= 0) { close(fd); fd = -1; }
            if (wasActive) {
                Lock lock(mMutex);
                std::memset(mSnapshot.pressed, 0, sizeof(mSnapshot.pressed));
                status(mSnapshot.buttons, State::Idle, 0, "Button monitoring paused");
                status(mSnapshot.usb, State::Idle, 0, "HID monitoring paused");
            }
            wasActive = false;
            delay(25);
            continue;
        }
        if (!wasActive) { nextOpen = 0; wasActive = true; }
        if (haveWork) {
            if (work.command == Command::SelectLed) ledWrite(work.argument);
            else storageProbe();
        }
        if (nowNs() >= nextOpen) {
            if (buttonFd < 0) {
                int ret = 0;
#ifdef CONFIG_ESP32S31_KORVO_ADC_BUTTONS
                buttonFd = open("/dev/buttons", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                ret = buttonFd < 0 ? -errno : 0;
                btn_buttonset_t supported = 0;
                btn_pollevents_s edges{15, 15};
                if (!ret) ret = ctl(buttonFd, BTNIOC_SUPPORTED, &supported);
                if (!ret && (supported & 15) != 15) ret = -ENOTSUP;
                if (!ret) ret = ctl(buttonFd, BTNIOC_POLLEVENTS, &edges);
                if (ret && buttonFd >= 0) { close(buttonFd); buttonFd = -1; }
#else
                ret = -ENOSYS;
#endif
                Lock lock(mMutex);
                status(mSnapshot.buttons, ret ? State::Unavailable : State::Ready, ret,
                       ret ? "Korvo ADC buttons unavailable" : "Native button monitoring");
            }
            const char* paths[2] = {"/dev/mouse0", "/dev/kbda"};
            for (unsigned i = 0; i < 2; ++i) if (hidFd[i] < 0) {
                if (i == 0 && !FACTORY_BACKEND_MOUSE_HOTPLUG_SAFE) continue;
                hidFd[i] = open(paths[i], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (hidFd[i] >= 0) {
                    Lock lock(mMutex);
                    if (!hidSeen[i]) ++mSnapshot.usbConnectCount;
                    hidSeen[i] = true;
                    std::snprintf(mSnapshot.usbDevice, sizeof(mSnapshot.usbDevice), "%s", i ? "Keyboard" : "Mouse");
                    status(mSnapshot.usb, State::Ready, 0, FACTORY_BACKEND_MOUSE_HOTPLUG_SAFE ?
                           "Native HID node opened" : "Keyboard opened; mouse driver blocked");
                } else {
                    int error = -errno;
                    Lock lock(mMutex);
                    if (error == -ENOENT || error == -ENODEV) {
                        if (hidSeen[i]) ++mSnapshot.usbDisconnectCount;
                        hidSeen[i] = false;
                    } else status(mSnapshot.usb, State::Error, error, "Native HID open failed");
                }
            }
            if (hidFd[0] < 0 && hidFd[1] < 0) {
                Lock lock(mMutex);
#ifdef CONFIG_USBHOST
                if (mSnapshot.usb.state != State::Error)
                    status(mSnapshot.usb, State::Idle, 0, FACTORY_BACKEND_MOUSE_HOTPLUG_SAFE ?
                           "Waiting for mouse/keyboard nodes" : "Waiting for keyboard; mouse driver blocked");
#else
                status(mSnapshot.usb, State::Unavailable, -ENOSYS, "Profile lacks USB host");
#endif
            }
            nextOpen = nowNs() + 1000000000ULL;
        }
        if (buttonFd >= 0) {
            btn_buttonset_t bits;
            ssize_t n = read(buttonFd, &bits, sizeof(bits));
            if (n == sizeof(bits)) {
                int selected = -1;
                {
                    Lock lock(mMutex);
                    for (unsigned i = 0; i < 4; ++i) {
                        bool down = bits & (1u << kButtonBits[i]);
                        if (down && !mSnapshot.pressed[i]) {
                            ++mSnapshot.buttonPressCount[i]; selected = i;
                        }
                        mSnapshot.pressed[i] = down;
                    }
                }
                if (selected >= 0) ledWrite(selected);
            } else if (n >= 0 || (errno != EAGAIN && errno != EINTR)) {
                int error = n < 0 ? -errno : -EIO;
                close(buttonFd); buttonFd = -1;
                Lock lock(mMutex);
                std::memset(mSnapshot.pressed, 0, sizeof(mSnapshot.pressed));
                status(mSnapshot.buttons, State::Error, error, "Button read failed");
            }
        }
        for (unsigned i = 0; i < 2; ++i) if (hidFd[i] >= 0) {
            // A single bounded read per node per iteration prevents a busy
            // keyboard/mouse from starving stop or button polling.
            mouse_report_s report{};
            uint8_t keys[16]{};
            ssize_t n = i ? read(hidFd[i], keys, sizeof(keys)) : read(hidFd[i], &report, sizeof(report));
            if (n > 0 && (i || n == sizeof(report))) {
                Lock lock(mMutex);
                mSnapshot.usbInputCount += i ? n : 1;
                std::snprintf(mSnapshot.usbDevice, sizeof(mSnapshot.usbDevice), "%s", i ? "Keyboard" : "Mouse");
                if (i) {
                    // Show translated bytes in hex. Never confuse them with raw
                    // HID usage codes or insert control sequences into UI text.
                    std::snprintf(mSnapshot.usbLastInput, sizeof(mSnapshot.usbLastInput),
                                  "Keyboard bytes: %zd, last 0x%02x", n, keys[n - 1]);
                } else {
                    // NuttX HID mouse has already accumulated/clamped these coordinates.
                    mSnapshot.mouseX = report.x; mSnapshot.mouseY = report.y;
                    mSnapshot.mouseLeft = report.buttons & MOUSE_BUTTON_1;
                    mSnapshot.mouseRight = report.buttons & MOUSE_BUTTON_2;
                    std::snprintf(mSnapshot.usbLastInput, sizeof(mSnapshot.usbLastInput),
                                  "Mouse x=%d y=%d L=%d R=%d", report.x, report.y,
                                  mSnapshot.mouseLeft, mSnapshot.mouseRight);
                }
                status(mSnapshot.usb, State::Ready, 0, FACTORY_BACKEND_MOUSE_HOTPLUG_SAFE ?
                       "Native HID input received" : "Keyboard input; mouse driver blocked");
            } else if (n >= 0 || (errno != EAGAIN && errno != EINTR)) {
                int error = n < 0 ? -errno : -EIO;
                close(hidFd[i]); hidFd[i] = -1;
                Lock lock(mMutex);
                bool disconnected = error == -ENODEV || error == -ENXIO || n == 0;
                if (disconnected && hidSeen[i]) ++mSnapshot.usbDisconnectCount;
                if (disconnected) hidSeen[i] = false;
                if (!i) mSnapshot.mouseLeft = mSnapshot.mouseRight = false;
                status(mSnapshot.usb, disconnected ? State::Idle : State::Error,
                       error, disconnected ? "Native HID disconnected" : "Native HID read failed");
            }
        }
        delay(20);
    }
    if (buttonFd >= 0) close(buttonFd);
    for (int fd : hidFd) if (fd >= 0) close(fd);
}
} // namespace
std::unique_ptr<FactoryBackend> createFactoryBackend()
{
    return std::unique_ptr<FactoryBackend>(new (std::nothrow) NativeBackend);
}
} // namespace os::app::factory
