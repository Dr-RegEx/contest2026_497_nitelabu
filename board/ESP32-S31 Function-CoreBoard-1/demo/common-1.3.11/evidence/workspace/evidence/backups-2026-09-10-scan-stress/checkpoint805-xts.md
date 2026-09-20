# S31 xTS checkpoint — 2026-09-15 12:05 +08:00

Verified common checklist: **22/35 (62.9%)**, not overall project completion.
Since787, full original C++ functions and real UART file send/receive passed.
The live ledger is xts-current-status.md; this checkpoint is an immutable
snapshot once covered by SHA256SUMS-xts805.

## Results and fixes

- NIST786 on781 finished188rows:162 numeric uniformity P-values all>0.0001,
  zero starred rows;26 RandomExcursions/Variant rows unavailable because
  only1 of10 streams had enough cycles.788 saved the original insufficient-
  cycle messages and free-memory output before reset. Not a full RNG PASS.
  The pervasive failures from770 disappeared after reenabling LP RNG at
  driver registration.44795df21f4 commits this clock fix, not a test change.
- C++ build787/flash789PASS;790 originalcxxtest abort caller61003de8 resolves
  to GCC uw_init_context_1's assertion that its own frame must be found.
  Build791/flash792PASS;793 prints EH header length0 and no matching FDE.
  Read-only layout reproduction795 finds180 section-address mismatches;
  orphan small-rodata/exception tables shift EH data by32bytes when the
  current loader rounds each allocated section to4bytes.
- Build794 groups RISC-V kernel ELF small-rodata/exception tables/small-data
  into explicit sections; retains EH registration and terminator. Layout795
  now5allocated sections,0mismatches. Temporary lib_abort.c/crt0 logging and
  SCHED_DUMP_ON_EXIT removed before validation. Flash796 hashPASS.
- **797 cxxtest PASS0.204s**: vector/map/C++17/RTTI and
  `Catch Exception: runtime error`, then clean NSH. No original test edited.
  Regression798helloxx3instancesPASS;799memory8/8PASS3.432s;
  800syscalls83/83PASS14.653s. NuttX7e73da9864a commits this group.
- Build801/flash804PASS with existing sb/rb YMODEM utilities. Host source
  inspection exposed incorrect partial-packet saved length and missing ACK
  for empty final batch header.802 unit regression reproduces3failures;
 803 passes both tests with128/129/1024/1025-byte payload coverage.
- **805 UART file send/receive PASS**:129-byte and65573-byte binary files
  upload in one batch, download individually, normal target exit, bytewise
  identical content.65573-byte SHA256:
  a8147e8e1451821e630e6283b21197d7d4ccbf847c90bd5496ebb824c5c049b2.
  No protocol rewrite: host uses apps/system/ymodem/sbrb.py. Original xTS
  document gives the functional criterion but omits its tutorial link;
  existing repository YMODEM is the explicit selected transfer mechanism.
  Files remain under fresh host xts805-ymodem-* and volatile /tmp/xts805.
  appsac43243d4 commits host fixes/tests;NuttX6c3a1973a61 adds test profile.

## Reproduction commands

Run from /home/regex/work/esp32s31-openvela:

```sh
D=backups/2026-09-10-scan-stress
S31_DEMO_PROFILE=demo-rmt-xts-cxx bash "$D/build-demo.sh" "$PWD/$D/build794-xts-cxx-sections.sha256" > "$D/logs/build794-xts-cxx-sections.log" 2>&1
python3 "$D/check-elf-layout.py" openvela-dev/out/esp32s31-cmake-demo/bin_debug/cxxtest > "$D/logs/xts795-cxx-layout.log" 2>&1
bash "$D/flash-demo-pair.sh" "$PWD/$D/build794-xts-cxx-sections.sha256" > "$D/logs/flash796-xts-cxx-sections.log" 2>&1
s31-reference/.venv-nuttx/bin/python -u "$D/xts-run-standard.py" cxxtest > "$D/logs/xts797-cxx-sections.log" 2>&1
s31-reference/.venv-nuttx/bin/python -u "$D/xts-run-standard.py" helloxx --no-boot > "$D/logs/xts798-helloxx-sections.log" 2>&1
s31-reference/.venv-nuttx/bin/python -u "$D/xts-run-standard.py" mm --no-boot > "$D/logs/xts799-mm-sections.log" 2>&1
s31-reference/.venv-nuttx/bin/python -u "$D/xts-run-standard.py" syscall --no-boot > "$D/logs/xts800-syscall-sections.log" 2>&1
S31_DEMO_PROFILE=demo-rmt-xts-ymodem bash "$D/build-demo.sh" "$PWD/$D/build801-xts-ymodem.sha256" > "$D/logs/build801-xts-ymodem.log" 2>&1
s31-reference/.venv-nuttx/bin/python openvela-dev/apps/system/ymodem/test_sbrb.py > "$D/logs/xts803-ymodem-host-after.log" 2>&1
bash "$D/flash-demo-pair.sh" "$PWD/$D/build801-xts-ymodem.sha256" > "$D/logs/flash804-xts-ymodem.log" 2>&1
s31-reference/.venv-nuttx/bin/python -u "$D/xts-ymodem.py" --tag xts805 > "$D/logs/xts805-ymodem.log" 2>&1
```

All new builds passed; actual first runtime failures and fixes above.788 is
read-only stats capture. Build791 is the C++ profile with temporary FDE lookup
diagnostic; archive retained. Earlier build/flash commands have the same
guarded pattern and their own numbered complete logs/paired SHA receipts.
Use new identifiers when rerunning; do not overwrite old evidence.

## Recovery / next work

Firmware791diagnostic,794C++PASS and801UARTPASS archived with paired kernel,
AppFS/config/map; C++ archives include debug ELF. Earlier787SHA set verified.
New incremental NuttX bundle starts at7631f4da431 (included in787 bundle),
apps bundle starts at8bce52fb6 (included in771 bundle). Preserve remaining
unrelated Wi-Fi/mqueue/monitor WIP. No reference repository edits/downloads,
efuse writes or partition changes. Flash remains0x2000/0x200000 only.

806 is building demo-rmt-xts-fsheap after these commits. Next is one
**supplemental**100-stream NIST run for eligible excursion coverage, with
400000bits/all15algorithms/original thresholds unchanged. It does not replace
the published10-stream786 record. Then hardware-crypto and remaining original
cases; cold power cycles/GPIO/BMI160/power measurement need real fixtures.
The new ELF check is only a regression guard for the proven loader mismatch,
and YMODEM host tests validate the required transfer tool; neither is counted
as an extra xTS heading.
