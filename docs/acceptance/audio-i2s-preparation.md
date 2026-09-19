# S31 I2S implementation candidate (not target tested)

The new `esp32s31_i2s.c/.h` exposes `esp32s31_i2s_initialize(0)`.
It uses the pinned S31 I2S HAL and AHB DMA LL, not the older S3 register
layout. Root owns board ES8311 registration and build integration.

## Initial boundary

- Single-core FLAT only. I2S0 and AHB GDMA group 0 pair 0 are exclusively
  reserved by the isolated audio profile. No DMA allocator or simultaneous
  DMA peripheral sharing is implemented.
- Half duplex: simultaneous capture and playback return `-EBUSY`.
- Philips format, 16-bit samples, mono or stereo. Mono TX copies to both
  I2S slots; mono RX selects the left slot. The board codec has one analog
  ADC/DAC channel, so stereo slots do not imply two independent analog paths.
- Fixed 12.288 MHz MCLK derived fractionally from XTAL. Rates 8, 12, 16,
  24, 32 and 48 kHz have integer BCLK division. 44.1 kHz and non-16-bit
  formats are unsupported and return the I2S API's zero result.
- Original xTS capture/playback at 16 kHz, mono, 16 bits is the first target.
  Codec reset's 48 kHz configuration is also accepted.
- Buffers are at most 32768 bytes and have a multiple-of-four byte size.
  Up to eight requests are queued. Empty TX end markers receive their normal
  asynchronous callback without starting DMA.

## Real transfer and lifetime behavior

One dedicated kernel thread owns the hardware, internal DRAM bounce buffer,
and descriptor chain. Application buffers may reside in PSRAM. Buffers are
referenced on queue insertion and released after exactly one callback.
AHB DMA interrupts report EOF/errors; the thread uses a finite semaphore
wait, verifies capture descriptor ownership/length, invalidates DMA-written
cache lines, and copies actual samples back. No synthetic capture data or
unconditional completion is returned. TX waits for I2S idle after DMA EOF.
STOP/SHUTDOWN cancel the active and queued generation, stop DMA, and disable
PA. PA is enabled only while actual TX is running and disabled after transfer
completion or failure. Unsupported ioctl commands return `-ENOTTY`.

The SRAM bounce buffer and descriptor storage are independently aligned to
64-byte cache lines and synchronized using the pinned cache API. A
non-cacheable SRAM range may return ESP_ERR_NOT_SUPPORTED from that API,
which correctly needs no cache action. Initialize verifies internal/DMA
address capabilities. UPDATE hardware handshakes are bounded to 10 ms.

## Known limitations and next proof

This is an initial buffer-at-a-time implementation; hardware is stopped
between APBs, so gapless capture/playback and duplex nxlooper are not yet
supported or claimed. The actual effect on original 10-second recordings and
audible playback must be inspected on target, and descriptor chaining across
APBs may be necessary if gaps are audible. Correct MCLK/BCLK/WS, actual DMA
interrupt delivery, codec microphone routing/gain and audible amplifier output
remain unverified. No waveform or original audio xTS PASS is claimed.

No UART, reset, firmware flashing, download, or shared build was performed by
this implementation agent. The ongoing longrun 864 was untouched.
