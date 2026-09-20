# Audio979: isolated original nxlooper implementation preparation

Status: CLEAN BUILD AND HOST LOGIC REVIEW PASSED; TARGET NOT RUN. No UART,
flash, reset, capture, playback or original xTS target PASS has occurred.
The frozen968 half-duplex image remains the fallback.

## Original test and endpoint mapping

Original xTS4.1.127 uses the existing nxlooper program, with these commands:

```
nxlooper
device pcm0p
device pcm0c
loopback 2 16 48000
stop
q
```

The original example's `pcm13c` maps to this board's `pcm0c`; playback is
`pcm0p`. The codec endpoints report INPUT or OUTPUT individually so nxlooper
selects separate capture/playback file descriptors. This is two 16-bit I2S
slots at48kHz, not two physical microphones: ES8311 has one ADC and one DAC.
Slot content and actual audio still require target verification. Do not
report stereo microphone support from a two-slot API configuration.

## Isolated implementation

Profile: `xts-flat-audio-duplex`. Options `ESP32S31_I2S_DUPLEX` and
`ES8311_SHARED_DUPLEX` are default-off. `SYSTEM_NXLOOPER` is explicitly on.
The original half-duplex transport remains selected when duplex is off.
The968 rate-dependent MCLK fix is preserved.

The replacement transport uses independent RX/TX request queues and four
512-byte circular DMA descriptors per direction. Each descriptor has its
own64-byte SRAM allocation. Payloads and descriptors must be DMA-capable,
internal and non-cacheable, checked before enabling hardware. No PSRAM
pointer is handed to DMA. RX is the48kHz master and remains active while
TX needs clocks, even if no capture APB is currently queued. TX is a slave;
its empty application queue is serviced with silence. The two clocks are
configured before startup, with RX configured last to retain MCLK ownership.

Only48kHz/16-bit/two-slot format is accepted. APB lengths must be positive,
no larger than32768 and exact multiples of512 bytes. The original nxlooper
profile uses4096-byte APBs, unchanged. Partial blocks are rejected rather
than padded and reported as an exact successful transfer.

A request completes only after all its DMA blocks complete; callbacks are
issued in submission order outside the driver mutex. Endpoint STOP cancels
only its queue. RX DMA remains as clock owner while TX is active. TX DMA
errors stop TX; RX errors invalidate both endpoints because RX owns clocks.
Descriptor starvation, FIFO errors and request timeouts are real failures;
they are not retried or hidden as successful buffers. An unstarted queued
request also has a progress watchdog allowing all accepted bytes ahead of
it at192000 bytes/second plus a1-second margin. No target fault-free claim
is made from these mechanisms alone.

ES8311 has one bus/address hardware lock, fixed endpoint directions,
configuration validation and active-direction ownership. Configuring a
second endpoint cannot reset/reclock the first one. Common power startup
runs for the first active endpoint. Normal STOP first drains its worker;
then its own serial interface is muted and its I2S endpoint stops. Common
reset/power-down happens only after the last active direction releases.
Pause/resume mute only that endpoint's interface. Message queue/thread
creation errors release any already acquired hardware ownership. RELEASE
stops an active worker and returns primed buffers if START failed; the
upper callback runs before dropping the final lower-half buffer reference.

## Required target qualification

After the actual24h longrun and evidence review release the board, qualify
this separate image with the original nxlooper command order above. Capture
must complete before playback starts, loopback must continue without DMA,
timeout or codec errors, and stop/quit must return cleanly. Preserve the
original serial output and actual listening evidence. The initial DMA ring
size and service latency, shared clock routing, mono codec slot contents and
analog audio behavior remain hardware-dependent and unverified. A clean
build is not an xTS pass or a guarantee of first-run success.

## Frozen build evidence

Final clean build979 kernel382484 bytes; receipt build979-flat-audio-duplex.sha256.
F0 dependency verification and source whitespace checks passed. DMA payload
g_data is2f0096c0; descriptors g_desc are2f00a6c0, both internal SRAM in the
link map. Actual-source host ring/STOP regression passed ASan/UBSan; it does
not model the hardware or ISR scheduling. With the actual968 configuration,
old/new ES8311 preprocessed source matches after normalizing assertion line
numbers only.968 and967 image receipts remain preserved.

TX completion denotes consumption of the request's DMA blocks. Continuous
stream STOP can truncate residual FIFO samples; this candidate does not claim
file-playback tail-drain completeness or arbitrary-format support. The original
continuous nxlooper/stop/quit scope and actual hardware gaps remain explicit.
