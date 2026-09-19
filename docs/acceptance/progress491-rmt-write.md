# Checkpoint 491: RMT write contract and Wi-Fi experiment closure

RMT wrapper now returns the underlying negative errno, validates nonzero
buffers and item-size alignment, rejects overflowing lengths, and handles
zero-byte writes without invoking hardware. This does not fix active-TX
cancellation, completion timeout, or semaphore ownership races.

Commands/results (run from workspace unless stated):

- `python3 openvela-dev/nuttx/tools/test_esp_rmt_write.py --revision 9d3b75798a3`
  reproduced the old swallowed-error assertion failure (host487 log).
- Current write/completion/WS2812 tests, nxstyle and git diff --check PASS
  (host488 retry). First style pass found an existing extra blank line;
  removed that line only, then reran successfully.
- `S31_DEMO_PROFILE=demo-rmt bash backups/2026-09-10-scan-stress/build-demo.sh
  /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build489-rmt-write.sha256`
  completed through image receipt and source-config restoration. No real
  build error in log. Original process exit result was lost across context
  recovery; independently checked no cmake/ninja process and both hashes OK.
- `bash backups/2026-09-10-scan-stress/flash-demo-pair.sh
  /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build489-rmt-write.sha256`
  flash490 exited 0; only 0x2000 and 0x200000, existing backup checked.
- `s31-reference/.venv-nuttx/bin/python -u
  openvela-dev/nuttx/tools/espressif/esp32s31_led_smoke.py
  --port /dev/ttyUSB0 --cycles 10`: LED491 exit 0, 40 frames, timer/console
  PASS. User reminded before test; optical result unverified.

Wi-Fi short-frame padding experiment482 failed all three cold-ARP TCP
trials (network484); padding and its test were archived then withdrawn.
No padding fix adopted. Diagnostic source restored exactly to480 before
the RMT edit. ARP-learning workaround helped b/g/n but not HE; neither is
full Wi-Fi acceptance. Windows capture awaits the previously requested
UAC authorization; no capture/filter/router/TUN change was made.

Stable Make432 restored by flash485, demo486 connected, HTTP487 passed100
samples and method/path/header/idle boundaries before flashing489. Board
now runs489 at NSH, not the HTTP server. Stable Make432 remains recoverable.

Nested HAL mbedTLS pre-existing namespace edits are documented and saved
in dependency476-nested-hal-audit.md and hal-mbedtls-existing-476.patch.
Reference was not modified; F0 PASS alone does not mean nested pristine.

Six Wi-Fi/monitor diagnostic source edits remain separate from RMT commit.
BLE/Zigbee remain later priority. Next bounded RMT work: propagate failure
of initial semaphore acquisition before any TX programming.
