# S31 xTS checkpoint — 2026-09-15 11:50 +08:00

Common checklist verified: **20/35 (57.1%)**, not total project completion.
Continue published original tests and usable hardware first. BLE/Zigbee last.

## Since checkpoint771

- NIST770 on765 finished with all188 report rows, but actual random-quality
  failures. Filesystem capacity is fixed; RNG is NOT accepted.
- Flash773 of771 hashPASS. Original ostest775 PASS52.081s, including timed
  message queues, signals, POSIX timer, RR, barrier, setjmp, schedlock and
  vfork; reaches user_main:Exiting and status0. No test assertions changed.
- NuttX7631f4da431f28cbf9953f5ac8bcebf9bd0afa18 commits the syscall-deferred
  signal recipient fix. Regression777memory8/8PASS,778scheduler9/9PASS.
- C++ build776/flash779PASS;780 originalcxxtest exits in exception phase;
  diagnostic782 confirms exit1. Neither the published earlier output nor
  successful helloxx is substituted for this failing full current test.
- RNG774 raw128-byte sample showed counter-like output. Source trace:
  esp_perip_clk_init -> periph_ll_clk_gate_set_default disables LP RNG after
  bootloader_random_enable. HAL esp_random also mixes RTC count, so output
  was nonzero despite the disabled RNG. Candidate esp_random.c reenables
  RNG once at device registration, after clock gating and before SMP start.
- Build781/flash783PASS.784host command exceeded the63-character NSH limit
  and was rejected before sending.784b shortened volatile pathname; sample
  no longer has the counter pattern.785originalsyscall83/83PASS14.876s.
- **786 full NIST is still running on781 at this checkpoint**: unchanged
  400000bits,10streams,all15 algorithms. No final result/no RNG PASS yet.
- Build787 C++ abort-caller diagnosticPASS, paired images and debug ELF
  archived. NOT FLASHED until786 finishes and releases the UART. Temporary
  lib_abort.c print and CONFIG_SCHED_DUMP_ON_EXIT must be removed after use.
- Added uncommitted demo-rmt-xts-ymodem profile for published UART file
  send/receive case1.3.11 using existing sb/rb and host sbrb.py. Not built or
  counted as passed yet. No new downloaded dependencies.

## Commands and complete logs

From /home/regex/work/esp32s31-openvela, with D=backups/2026-09-10-scan-stress:

```sh
S31_DEMO_PROFILE=demo-rmt-xts-cxx bash "$D/build-demo.sh" "$PWD/$D/build776-xts-cxx-exit.sha256" > "$D/logs/build776-xts-cxx-exit.log" 2>&1
S31_DEMO_PROFILE=demo-rmt-xts-fsheap bash "$D/build-demo.sh" "$PWD/$D/build781-xts-rng-clock.sha256" > "$D/logs/build781-xts-rng-clock.log" 2>&1
S31_DEMO_PROFILE=demo-rmt-xts-cxx bash "$D/build-demo.sh" "$PWD/$D/build787-xts-cxx-abort.sha256" > "$D/logs/build787-xts-cxx-abort.log" 2>&1
bash "$D/flash-demo-pair.sh" "$PWD/$D/build781-xts-rng-clock.sha256" > "$D/logs/flash783-xts-rng-clock.log" 2>&1
s31-reference/.venv-nuttx/bin/python -u "$D/xts-run-standard.py" ostest > "$D/logs/xts775-ostest-sigpending.log" 2>&1
s31-reference/.venv-nuttx/bin/python -u "$D/xts-run-standard.py" syscall --no-boot > "$D/logs/xts785-syscall-sigpending.log" 2>&1
s31-reference/.venv-nuttx/bin/python -u "$D/xts-nist.py" > "$D/logs/xts786-nist-rng-clock.log" 2>&1
```

Build logs retain actual expanded configure/build commands; successful builds
have paired SHA receipts. No reset/clean/reference modification. Flash guard
verifies the existing identical5MiB backups and image receipts, only writes
kernel0x2000 and AppFS0x200000; persistent0x500000+ and efuses untouched.

Earlier SHA256SUMS-xts771 and bundles remain unchanged. Firmware771,776,781,
787 pairs now archived. Uncommitted C++/RNG/older Wi-Fi/mqueue work is retained;
do not reset it. Active build directory contains787 while the board runs781.

Backup note: the first untracked787 tar attempt used positional -C too late
and failed without altering sources. The valid replacement is
nuttx-untracked787b.tar.gz; all18 entries were listed successfully. The failed
empty archive is retained but intentionally excluded from the SHA manifest.
