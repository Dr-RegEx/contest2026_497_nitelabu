// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

namespace os::app::factory {
// UI-only interface. Implementations use native openvela/POSIX services and must
// never call LVGL. All strings below are bounded, NUL-terminated UTF-8 snapshots.
enum class State : uint8_t { Unavailable, Idle, Starting, Ready, Busy, Error };
struct Status {
    State state = State::Unavailable;
    int error = 0; // 0 or negative errno; availability is explicit in state.
    char text[96] = "Unavailable";
};
enum class AudioState : uint8_t { Unavailable, Idle, Recording, Playing, Stopping, Error };
struct Snapshot {
    uint64_t sampledNs = 0; // CLOCK_MONOTONIC; 0 means no sample yet.
    Status camera;
    Status digit;
    bool digitDisplayOwned = false;
    uint32_t cameraWidth = 0, cameraHeight = 0;
    uint64_t capturedFrames = 0, cameraDroppedFrames = 0;
    Status audio;
    AudioState audioState = AudioState::Unavailable;
    uint64_t recordedBytes = 0, audioCapacityBytes = 0, audioPositionMs = 0;
    bool hasRecording = false;
    Status storage;
    bool sdMounted = false, sdProbePassed = false;
    uint64_t sdFreeBytes = 0, sdProbeCount = 0;
    Status buttons;
    bool pressed[4] = {};
    uint64_t buttonPressCount[4] = {};
    Status led;
    int activeLedButton = -1; // Set only after a successful device write.
    uint8_t ledRed = 0, ledGreen = 0, ledBlue = 0;
    Status usb;
    uint64_t usbConnectCount = 0, usbDisconnectCount = 0, usbInputCount = 0;
    char usbDevice[24] = "None", usbLastInput[64] = "No input";
    int mouseX = 0, mouseY = 0;
    bool mouseLeft = false, mouseRight = false;
};

struct CameraFrame {
    const uint8_t* data = nullptr; // Immutable native-endian RGB565, no padding pixels.
    size_t size = 0, stride = 0; // Row padding allowed; size >= stride * height.
    uint32_t width = 0, height = 0; // Must be 240 x 240 for this UI.
    uint64_t captureNs = 0, sequence = 0; // Real monotonic timestamp and capture count.
    uintptr_t lease = 0; // Opaque nonzero token, valid until releaseFrame().
};

enum class Command : uint8_t {
    StartRecording, StopAudio, PlayRecording, ResetAudio, ProbeStorage, SelectLed, StartDigit, StopDigit
};

class FactoryBackend {
public:
    virtual ~FactoryBackend() = default;
    // start()/setActive()/shutdown()/destruction run on the lifecycle worker.
    // All except shutdown() are nonblocking. The remaining methods run on the
    // UI while its lease is ready, concurrently with the worker's setActive().
    // Returns 0 if queued, negative errno if rejected. Never implies completion.
    virtual int start() = 0;
    virtual int setActive(bool active) = 0;
    virtual int setCameraPreview(bool enabled) = 0;
    virtual int submit(Command command, int argument = 0) = 0;
    // Nonblocking coherent copy. -EAGAIN leaves the UI's previous snapshot intact.
    virtual int snapshot(Snapshot& output) = 0;
    // At most one outstanding lease, -EAGAIN when no new completed frame exists.
    virtual int acquireFrame(CameraFrame& output) = 0;
    virtual void releaseFrame(uintptr_t lease) = 0;
    // Called without outstanding frame/UI leases, on the lifecycle worker.
    // Stop/join every worker and drain callbacks without requiring UI progress.
    // Idempotent: the native destructor calls shutdown() again. MEDIA has no
    // established finite shutdown deadline; retain ownership until this returns.
    // A timeout must never authorize deletion, detachment or a replacement.
    virtual void shutdown() = 0;
};
// Native implementation supplies this symbol. Called only by the managed worker.
// Return null on allocation failure; do not throw. No references to UI storage.
std::unique_ptr<FactoryBackend> createFactoryBackend();
}
