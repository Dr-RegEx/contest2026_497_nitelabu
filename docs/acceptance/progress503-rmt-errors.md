# RMT error-path groups 492–503

Committed NuttX 8a2a8f8f019 (parent532aa75837e), Apps unchanged ae0dfd89c.
Initial tx_sem acquisition now propagates errors before programming TX;
IRAM buffer rejection uses -EINVAL, not positive ESP_ERR_INVALID_ARG.
Host492 old-source runtime assertion FAIL; host493 fixed actual-function
model PASS for EINTR/ECANCELED/EINVAL, no register writes or semaphore post,
IRAM rejection and success. Completion cancellation remains out of scope.

Exact build command:
`S31_DEMO_PROFILE=demo-rmt bash backups/2026-09-10-scan-stress/build-demo.sh
/home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build494-rmt-acquire.sha256`
Log build494-rmt-acquire.log, process exit0, first real build error: none.
flash497 uses flash-demo-pair.sh with that absolute receipt; exit0.
LED498 uses esp32s31_led_smoke.py --port /dev/ttyUSB0 --cycles 10,
exit0 and 40frames PASS, optical unverified.
Demo499 uses esp32s31_demo.py --port /dev/ttyUSB0 --log
backups/2026-09-10-scan-stress/logs/demo499-rmt-acquire.log, exit0,
PID12, 192.168.1.60:8080. Hidden credentials, no credential files.
HTTP500 Windows direct test_http.py 192.168.1.60 --samples 100 --boundaries
completed exit0,100 samples and boundaries PASS before flash504.

I2C495 ran on prior firmware489, not494, concurrently with the offline
build and finished before flashing: esp32s31_i2c_smoke.py --port /dev/ttyUSB0
--no-boot --batches 5 --reads 128 --nack-recovery. Exit0,3840 ID reads,
15 resets,4 NACK recovery passes. Only100/400kHz in this run, no audio writes.
Host496 broader regression suite exit0 including F0; protocol test explicitly
used historical6e102 because current diagnostic frame counters are excluded.
Nested HAL audit476 still applies; F0 does not certify nested cleanliness.

Next two-line fix (uncommitted at this note): esp_rmtinitialize frees priv
after rmt_config or rmt_driver_install failure. Newtest test_esp_rmt_initialize.py.
First host501 attempt had a fixture extraction compile error: the helper
omitted the preceding multiline return type. Fixed fixture prefix, then
host501 retry reproduced actual old-source allocation-leak assertion.
Host502 current initialization/acquire/write/completion/WS2812 lifecycle/write,
nxstyle, diff-check all PASS. Does not claim driver_install internal rollback.
Build503 command identical to494 except receipt build503-rmt-init.sha256;
full log build503-rmt-init.log. Completed exit0; first real build error:none.
flash504 with build503 receipt completed exit0, same two authorized offsets.
LED505 exited0,40frames plus timer/console PASS, optical unverified.
The two-line initialization cleanup is now committed after8a2a; exact HEAD
is recorded in checkpoint505/nuttx-head.txt. Board currently runs503 at NSH.
Firmware494 was not separately archived before rebuild503; its receipt/logs
are retained, and503 includes both fixes. Earlier489/Make432 backups remain.

Wi-Fi/monitor diagnostics remain six separate uncommitted files. Pending
Windows UAC capture request has no answer; no administrative capture begun.
RMT full cancellation/refill/SMP review in rmt-followup494.md. Do not claim
full Wi-Fi or RMT acceptance from the successful bounded smoke tests.
