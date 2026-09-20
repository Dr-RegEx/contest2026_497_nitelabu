# S31 original nxlooper: bounded full-duplex feasibility review

**Historical feasibility review. September16 candidate979 now implements the
scoped duplex transport/shared codec path and has passed clean build and host
logic checks. Hardware qualification is still pending. See
audio-duplex-979-preparation.md; the original analysis below is retained as
design rationale, not the current implementation status.**

Status: SOURCE AUDIT ONLY. No candidate image, configuration, driver change,
UART access, reset or target PASS was produced. Audio963 remains unchanged.

## Original acceptance and actual caller ordering

The local original xTS document, section 4.1.127 (line 4548), runs nxlooper
with `loopback 2 16 48000`, then stop and quit. Its example capture node is
pcm13c; this board currently registers pcm0c. Any board-specific node mapping
must be recorded rather than silently represented as the literal example.

`apps/system/nxlooper/nxlooper.c` starts capture first (line 477), waits for a
completed capture buffer, then starts playback (line 579). Stop is playback
first, capture second (line 610). Therefore a TX-master design that only
generates clocks after an application playback buffer arrives can deadlock
before the first capture completes. This is a concrete ordering requirement,
not a reason to change the original nxlooper program.

## Transport changes that are actually required

The present `arch/risc-v/src/esp32s31/esp32s31_i2s.c` owns one request queue,
completion semaphore, generation, 32 KiB bounce buffer and descriptor array.
Its interrupt disables both directional interrupts. Every completed request
stops both I2S directions and resets both GDMA channels. These mechanisms
must be replaced, not merely have the opposite-direction EBUSY removed.

An isolated implementation needs two queues, independent DMA buffers,
descriptors, completion/error state and cancellation generations. A STOP
must identify its endpoint: the present shared i2s_dev ioctl has no direction
argument. Distinct endpoint wrappers or an explicitly scoped lower-half
contract are needed. IRQ and timeout cleanup must affect only the owning
direction and return each accepted buffer exactly once.

For the 48 kHz / 16-bit / two-slot scope, RX-master/TX-slave is consistent
with nxlooper's start order. It still requires continuous RX clock ownership
while TX is consuming a prior buffer. Simply cloning the present worker is
insufficient: it stops and reconfigures RX at every buffer boundary. If the
master is stopped or its data queue drains, an active slave can stall. A
continuous DMA ring or an explicitly coordinated clock-preserving mechanism
needs real underrun/overflow handling and cancellation. This is a new stream
engine, not a small opt-in register patch.

Pinned IDF evidence:

- `components/esp_driver_i2s/i2s_std.c:156` enables shared BCK/WS.
- `i2s_std.c:306` changes the later initialized master to a slave in full
  duplex; both directions are not independently clock-driving masters.
- `i2s_std.c:205` binds MCLK to the master direction.
- `i2s_common.c:565` links DMA descriptors in a circle. Start/reset is a
  channel operation, not performed after every application audio buffer.

Pinned HAL evidence is under `components/esp_hal_i2s`. Both
`i2s_hal_set_tx_clock` and `i2s_hal_set_rx_clock` bind MCLK to their own
direction when passed non-null clock information (`i2s_hal.c:107,125`). A
duplex implementation must avoid letting a later slave setup steal this
binding. The S31 LL implements shared BCK/WS through `tx_conf.sig_loopback`.
The existing half-duplex clock setup is not missing this binding: HAL already
performs it, so there is no justified standalone MCLK fix to apply to963.

## Codec lifecycle is a separate required change

`boards/.../src/esp32s31_audio.c` creates two ES8311 lower halves at address
0x18, sharing one physical codec. Their private locks do not protect shared
codec register state. `drivers/audio/es8311.c` currently:

- Resets the whole codec on either stream configure (1047,1108), shutdown
  (1166), worker exit (2147) and instance initialization (2328).
- Writes common ADC/DAC power registers on either start (around1487).
- Powers down both ADC/DAC paths before joining either stopping worker
  (around1608), although the other worker may still own in-flight buffers.
- Changes both I2S sample rates and widths when configuring either instance.

The minimum opt-in codec change requires a shared hardware object keyed by
I2C bus/address, a shared mutex, configured/active direction ownership,
common rate/slot validation, reset only when both streams are inactive, and
direction-local interface mute/disable. Shared power-down must happen only
after the final active stream drains or is cancelled. Error unwind on failed
message-queue/thread creation must release this ownership. Initialization,
pause/resume, natural completion, shutdown and explicit stop all have to use
the same lifecycle; guarding only the obvious stop function is incomplete.

The public driver changes would need a new default-off option and preserve
the existing path when disabled. This shared lifecycle plus the replacement
transport is beyond a small, evidence-backed candidate that can safely be
prepared without its first hardware playback/capture qualification.

## Decision and next dependency

Do not add an empty duplex profile, enable nxlooper alone, or remove EBUSY to
create an apparent build milestone. Preserve963 for original independent
playback and capture qualification after the long test releases the board.
Full duplex is feasible in principle but remains an explicit implementation
gap. A later implementation should isolate the transport in a separate
default-off profile and complete both the directional stream engine and the
shared codec lifecycle together. Hardware evidence must then include original
nxlooper start/stop and actual audio; a successful compile is not acceptance.
