# Candidate 916: original audio test preparation

BUILD PASS ONLY; no target boot, DMA, recording, playback, sound-quality or xTS PASS.

Independent profile: xts-flat-audio. Output: openvela-dev/out/esp32s31-xts-flat-audio.
Image: 421572 bytes. Receipt: build916-flat-audio.sha256.
Archive: firmware916-audio-build-only.tar.gz; SHA256SUMS-xts916.

S31 I2S0 uses real AHB GDMA pair 0, IRQ completion, bounded waits, static internal
DMA storage and asynchronous APBs. Initial half-duplex Philips 16-bit mono/stereo;
8/12/16/24/32/48 kHz clock family, fixed 12.288 MHz MCLK. No 24/32-bit samples,
44.1 kHz, simultaneous capture/playback, or verified seamless streaming claim.

Board codec: I2C0 SCL50/SDA51, address0x18; MCLK52, BCLK53, RX54, WS55, TX56,
PA57. PA off at boot and idle, active during TX. Raw nodes pcm0p and pcm0c.
The existing ES8311 driver required mutex declarations, PCM capability reporting,
actual configuration return-code propagation, I2S channel selection, zero-result
rejection, and propagation of stream errors to the upper half. Synchronous I2S
submission failure returns the queued buffer and terminates with failure.

Original target priority: 4.2.13 single-channel capture (document line2852),
4.2.10 single-channel playback (line2925), 16kHz/16-bit/mono, ten-second source.
Use the original cmocka_driver_audio command and retain actual recorded data and
audible playback evidence. A successful API return alone does not certify sound.
Prepare writable /data after the active longrun; do not format persistent storage
implicitly. The profile already mounts volatile /tmp; a separate temporary /data
may be mounted explicitly if sufficient free memory is confirmed.

Dependency lock verification PASS; full CMake build/link, original test symbol
checks, receipt checks and git diff --check PASS. Original HAL/IDF remain unchanged.
Candidate912 failed linking with DRIVERS_AUDIO absent; 913 exposed a missing mutex
include; 914 compiled but was superseded for capability/error fixes. Their logs
remain intact. No candidate was flashed. Keep longrun864 uninterrupted.

916 additionally rejects enqueue during termination and returns saved transfer errors from STOP, preventing the original test from waiting forever or hiding DMA failure. Read-only peer review identified this path; no original test application was modified. 915 remains archived and is superseded.
