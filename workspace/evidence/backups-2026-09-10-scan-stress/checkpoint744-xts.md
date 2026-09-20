# xTS checkpoint744 — 2026-09-14

18/35 common checklist headings verified (51.4% common coverage, not total
project completion). This checkpoint extends checkpoint711 and checkpoint726.

## New actual standard test results

- 732 pipe: PASS33.542s, FIFO/interlock/transfers/redirection all complete.
- 733 popen: PASS0.314s, help output and Calling pclose(), normal NSH return.
- 734 md5_test -f /etc/1.txt -c100:100/100 identical hashes, also compared
  against host MD5 of the fixture. Fixture lives in new volatile /etc tmpfs.
- 736 footprint: free/df-h on both CPUs, actual CPU/mask in proc status,
  original affinity restored. Physical map in xts736-footprint.md.
- 743 cmocka_driver_timer -d /dev/timer0:1/1PASS5.591s; start/stop/status/
  notification tested by original source. Registered general timer, not a
  replacement for the existing RISC-V system-tick implementation.
- 744 cmocka_driver_uart -d /dev/ttyS0 -n0/1/2:3/3PASS, original payload,
 10 random <=100-byte bursts as in the published host generator. Current
  source uses separate modes unlike the old combined manual instructions.

735: host parser expected bare init but ps prints /system/bin/init; failed
before changing affinity. Fixed host parsing;736 complete. Not a board fault.
740: read-only `taskset 1 free` additionally validates system() external-shell
spawn fix, not another xTS heading and not additional progress credit.

## Build commands, first actual blockers and outcomes

All invoked from /home/regex/work/esp32s31-openvela using:

`S31_DEMO_PROFILE=<profile> bash backups/2026-09-10-scan-stress/build-demo.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build<N>-<suffix>.sha256 > backups/2026-09-10-scan-stress/logs/build<N>-<suffix>.log 2>&1`

| N / suffix | Profile | First error / outcome |
|---|---|---|
|727 / xts-io|demo-rmt-xts-io|system.c146 implicit task_spawn; SHPATH Kconfig excludes BUILD_KERNEL.|
|728 / xts-io|demo-rmt-xts-io|compile fixed; guard fails missing TESTS_TESTCASES: existing tests repo lacks top-level CMake.|
|729 / xts-io|demo-rmt-xts-io|CMake registration added; stale generated apps/Kconfig still omits tests. Guard catches missing config again.|
|730 / xts-io|demo-rmt-xts-io|old generated Kconfig moved to apps-Kconfig-before730 (not deleted); regeneration succeeds, all required binaries present, exit0.|
|737 / xts-drivers|demo-rmt-xts-drivers|esp_timer.c114 old TG0 interrupt identifier undefined; S31 uses ETS_TIMERGRP and different RCC/group-index clock interface.|
|738 / xts-drivers|demo-rmt-xts-drivers|timer compiles; original block test links private find_mtddriver/find_blockdriver in a user ELF. Existing Makefile already limits this test to FLAT; CMake corrected to match.|
|739 / xts-drivers|demo-rmt-xts-drivers|compile succeeds; guard rejects missing SIG_EVTHREAD. Current NuttX sched/Kconfig explicitly depends !BUILD_KERNEL. RTC full suite cannot be enabled here; do not force a privileged callback into user memory.|
|741 / xts-drivers|demo-rmt-xts-drivers|timer/UART batch separated; required full RTC profile retained as demo-rmt-xts-rtc. Build exit0, verified binaries/receipt.|
|745 / xts-rng|demo-rmt-xts-rng|existing patched local NIST source accepted by CMake, no fetch; exit0. Source review reveals required template9 asset absent from old doc setup.|
|746 / xts-rng|demo-rmt-xts-rng|original local template9 staged in AppFS; build exit0 and byte-for-byte cmp passes. No NIST test algorithm edits.|

Flash command for731/742/747, receipts730/741/746 respectively:
`bash backups/2026-09-10-scan-stress/flash-demo-pair.sh backups/2026-09-10-scan-stress/build<N>-<suffix>.sha256 > backups/2026-09-10-scan-stress/logs/flash<M>-<suffix>.log 2>&1`
731/742exit0, both written hashes verified.747ongoing at checkpoint creation.

Target command for732/733/734/743:
`s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-run-standard.py <pipe|popen|md5_test|timer> > backups/2026-09-10-scan-stress/logs/xts<N>-<case>.log 2>&1`
735/736 use xts-footprint.py;744 uses xts-uart.py, same interpreter/log pattern.
All successful target verdicts inspect original results, not merely main exit.

## Additional repositories / commits

- NuttX09a5ac987cd constructor symbols, fd5f113e1a0 reboot.
- NuttXff86dc44073 original pipe/popen/MD5 profile.
- NuttX70e318252c0 S31 timer RCC/IRQ adaptation, board init and profiles;
  target743 and style/diff checks pass. Existing mqueue WIP excluded.
- Apps66d5946b8 system shell Kconfig (740 target proof).
- Apps32fcd1f65 CMake block FLAT restriction matches existing Make.
- Existing tests repo branchcodex/esp32s31-xts-cmake, baseline
 4cd233d7f98efe6356cb43dc3e9910ce354f4fe9, commit7bdeb9e CMake registration
  around unmodified filesystem testcase sources. Existing generated Kconfig
  and .kconfig changes untouched/uncommitted.
- Existing libcxx27cf67588 from checkpoint726 remains part of delivery.

## Pending / no false completion

RTC full suite requires unavailable SIGEV_THREAD and a periodic lower-half
implementation. Kernel heap/ostest/block tests still need compatible FLAT
test mode or proper privileged test integration. C++ exception WIP is not a
verified fix; fstest capacity warning remains partial. External fixtures,
manual reset/power-cycle,12h/24h tests unverified. BLE/Zigbee remain last.
NIST planned per original400000/10stream/alltests, with local template copied
to fresh /tmp/templates. No RNG PASS before complete statistical report.

Firmware730 and741 archived before next builds. No reference repository,
efuse or raw writable-data-region changes. Last verified board741,747 now
replaces only authorized paired slots with746. Backup hashes inSHA256SUMS-xts730
cover previous checkpoints/recovery; later checkpoint artifacts kept separately.
