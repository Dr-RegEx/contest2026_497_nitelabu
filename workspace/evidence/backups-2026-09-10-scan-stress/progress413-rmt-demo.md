# RMT / onboard RGB checkpoint

NuttX commits: `3199c5028dd` (HAL compatibility, WS2812 lifetime and TX
completion ordering), `ae2de864dc9` (board/profile/smoke/docs).
Apps commit: `f5284268f` (bounded low-brightness s31led).
Older six Wi-Fi/SMP diagnostic edits and three profiles remain separate.

## Build sequence and first real errors

- 398: removed locked-HAL getters, first `rmt_ll_rx_get_mem_blocks`.
  Added a private S31 compatibility header based on the locked register fields.
- 399: driver CMake build passed, before application/lifetime additions.
- 400 and 402: configuration guard rejected missing EXAMPLES_S31LED; cached
  CMake application menu did not discover the new application. Regenerating
  Make's examples menu alone was insufficient.
- Generated CMake apps/Kconfig was moved to cmake-apps-Kconfig-before403,
  not deleted; CMake regenerated its own menu.
- 403: missing include/nuttx/config.h after resetconfig. Build script now
  explicitly reruns CMake configuration after resetconfig, before Ninja.
- 405: built, but exposed private HAL LL definitions through public esp_rmt.h,
  causing board-target undefined HAL config warnings.
- 406: passed after restoring the public header and keeping compatibility
  includes private. All boolean checks and protocol 7 passed; AppFS contains
  s31led with checked ELF metadata/hash. Existing unrelated warnings remain.

Build command (full underlying commands are traced in build logs):

```sh
S31_DEMO_PROFILE=demo-rmt bash backups/2026-09-10-scan-stress/build-demo.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build406-led-private-hal.sha256
bash backups/2026-09-10-scan-stress/flash-demo-pair.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build406-led-private-hal.sha256
```

Flash 407 exited 0 with both hashes verified, after the existing first-5-MiB
backup checks. Only kernel 0x2000 and AppFS 0x200000 were written.

## Board results

- 408: boot, /dev/leds0, s31led 1 (4 frames), timer and status PASS.
- 409: authorized b/g/n association, DHCP, gateway and demo startup PASS.
- 411: s31led 10 (40 frames) under HTTP load PASS, no reboot.
- 412: 3840 codec ID reads, 15 resets, four NACK/retry sequences PASS.
- 410: 100 HTTP samples and method/path/header/idle boundary checks PASS;
  overlapped 411 then 412. All sessions explicitly exited 0.

Exact serial commands, with output in logs/led408-first.log,
logs/led411-http.log and logs/i2c412-rmt-http.log respectively:

```sh
s31-reference/.venv-nuttx/bin/python -u openvela-dev/nuttx/tools/espressif/esp32s31_led_smoke.py --port /dev/ttyUSB0 --cycles 1
s31-reference/.venv-nuttx/bin/python -u openvela-dev/nuttx/tools/espressif/esp32s31_led_smoke.py --port /dev/ttyUSB0 --no-boot --cycles 10
s31-reference/.venv-nuttx/bin/python -u openvela-dev/nuttx/tools/espressif/esp32s31_i2c_smoke.py --port /dev/ttyUSB0 --no-boot --batches 5 --reads 128 --nack-recovery
```

HTTP client: existing Windows Python running Apps examples/s31demo/test_http.py
192.168.1.60 --samples 100 --boundaries, direct local client, no proxy change.
Demo startup used tools/espressif/esp32s31_demo.py with hidden credentials,
redacted logs/demo409-rmt.log. No credentials persisted.

Host tests test_esp32s31_rmt_compat.py, test_esp_ws2812_lifecycle.py,
test_esp_rmt_completion.py and Apps examples/s31led/test_app.py all passed.
Diff whitespace checks and nxstyle on new compatibility header/app passed.

## Scope and next work

Board remains on matching 406 kernel/AppFS with HTTP demo PID 12 at
http://192.168.1.60:8080/ and LED off. Optical colors remain unverified pending
observation. Single-channel single-pixel TX only; no RX, DMA, long streams,
loop mode, error cancellation, full cross-hart or physical timing acceptance.
Full Make build with I2C/RMT remains untested. Existing low-level RMT waits
ignore interruption and can wait indefinitely; generic WS2812 offset/error
paths require further work. Continue these as separate reviewable changes.
Wi-Fi b/g/n demo usable, HE and full stability incomplete. BLE/Zigbee are
scheduled last, not removed. No reference/dependency/original document edits.

User follow-up: working remotely and unable to observe the LED now. On the
next restart, announce the optical check before repeating the low-brightness
sequence. Do not treat the missing observation as a blocker or an optical PASS.
