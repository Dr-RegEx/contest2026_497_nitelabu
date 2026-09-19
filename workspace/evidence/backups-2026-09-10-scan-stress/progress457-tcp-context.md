# S31 checkpoint through experiment 457

NuttX branch codex/esp32s31-port, HEAD 9d3b75798a3.
Apps branch codex/esp32s31-nettest, HEAD ae0dfd89c.
No unresolved Git index entries or active merge/rebase/cherry-pick state.
The six older Wi-Fi/SMP diagnostic edits remain deliberately uncommitted.

## Verified recovery baseline

Make432 image pair (fixed out/esp32s31-make-rmt420/nuttx) passed flash434,
LED435/440, BOOT436, demo437, HTTP438 (100 requests plus boundary cases),
and I2C439 (3840 reads, 15 controller resets, four NACK recovery checks).
Idle441 showed stable bounded memory samples and ongoing interrupt activity,
not evidence of an idle interrupt storm. These are bounded checks, not formal
xTS certification. RGB optical acceptance is still pending: the remote user
requested a reminder and repeat after restart. LED456 was announced and
repeated (40 frames, final off); serial PASS is not optical confirmation.
Make432 remains recoverable via flash-make434-pair.sh and checkpoint433.

## Wi-Fi latency investigation

Build443 introduces monitor-forwarding-to-Wi-Fi-callback cycle measurement;
host442-retry passes. It does not measure radio arrival or the entire hardware
interrupt entry. Build447 separates direct and deferred samples (host446 PASS).
Both build443/447 passed and were flashed (444/448).
Build451 enables HE only relative to 447's configuration; it passed artifact
checks (protocol bitmap 71), was archived, but has NOT been flashed.

Three-round gateway/TCP experiments, b/g/n bitmap 7:

- 445 automatic AP: all gateway checks pass; TCP FAIL/PASS/FAIL.
- 449 fixed weaker AP: all gateway checks pass; TCP FAIL/FAIL/PASS.
- 450 fixed stronger AP: all gateway checks pass; TCP FAIL/FAIL/FAIL.
- 449 round 1 TCP fails despite no recorded callback latency above 1 ms.
  Thus a long measured latency tail is not a necessary condition for failure.
  Stronger RSSI also does not resolve the observed TCP failures.

No claim of a proven HE, TUN, router or interrupt-ownership root cause.
Temporary diagnostics, test_esp32s31_wifi_latency.py and HE profile are WIP.

## Confirmed TCP address-context defect and bounded fix

Commit 9d3b75798a3 changes only net/tcp/tcp_accept.c and its host test.
Previously the asynchronous accept callback wrote caller-supplied sockaddr
and socklen buffers. With separate address environments, the callback may
execute under another task's mappings. The fix retains the accepted kernel
connection in the callback and copies its peer address in the accepting task,
after a successful wait (also the common path for pending backlog entries).
No new allocation, packet logging, timeout increase or protocol change.

Commands/results (full build command trace in logs/build454-demo.log):

```sh
python3 tools/test_tcp_accept_address_context.py --revision 77f64f40ae3
# host452: expected FAIL, assertion that peer-address copy uses caller context.
python3 tools/test_tcp_accept_address_context.py
# host453: PASS with TCP backlog enabled and disabled, UBSan.
tools/nxstyle net/tcp/tcp_accept.c
git diff --check
# Both PASS.
S31_DEMO_PROFILE=demo-rmt bash backups/2026-09-10-scan-stress/build-demo.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build454-demo.sha256
# PASS; first real build error: none. Existing HAL warnings remain.
bash backups/2026-09-10-scan-stress/flash-demo-pair.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build454-demo.sha256
# flash455 PASS after backup/hash checks; only 0x2000 and 0x200000.
s31-reference/.venv-nuttx/bin/python -u openvela-dev/nuttx/tools/espressif/esp32s31_led_smoke.py --port /dev/ttyUSB0 --cycles 10
# led456 PASS, optical unverified.
S31_EXPECT_PROTOCOL=7 S31_TEST_BSSID=60:ce:41:ab:02:d0 s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/network-repeat.py network457-accept-fixed tcp-diag
# Private hidden credentials; gateway all PASS; TCP FAIL/PASS/FAIL.
```

Host test extracts actual control-flow functions with modeled callback/task
contexts; it is not an MMU emulator. Hardware round 2 verified 4096-byte TCP
send/echo/close with the fix. The callback runs AFTER handshake completion,
so this real safety defect is not sufficient to explain connect timeouts.

## Exact handoff state

Board runs diagnostic454; 457 cleanup resets and disables WLAN, so HTTP demo
is not running. Firmware454 is archived separately. A new build458 is being
prepared with existing NET_STATISTICS enabled for /proc/net/tcp and net/stat;
it has not yet been accepted or flashed at this checkpoint. Network probe
now supports opt-in S31_TCP_PROC_DIAG=1 snapshots; 457 did not use them.

Complete adaptation remains unfinished: Wi-Fi HE/stability, RMT timeout and
cancellation paths, I2S/audio and other peripherals, full SMP/MMU acceptance,
storage endurance and formal xTS. BLE/Zigbee remain scheduled last, not removed.
No new dependencies downloaded; reference sources and original documents
unchanged; no security fuses or flash regions at/above 0x500000 modified.
