# xTS-first checkpoint through711 — 2026-09-14

Common checklist verified coverage:8/35=22.9%, not whole-project completion.
Authoritative ledger: xts-current-status.md. No custom diagnostic counted as xTS.

## Standard target outcomes

| Case | Evidence | Result |
|---|---|---|
| 1.1.1 | xts710-mm.log,704firmware | 8/8PASS,clean exit |
| 1.1.2 | xts711-sched.log,704firmware | 9/9PASS,clean exit |
| 1.1.3 | xts709-syscall.log,704firmware | 83/83PASS,clean exit |
| 1.1.5 | xts693-getprime.log,690firmware | PASS,1040ms |
| 1.1.7 | xts694-scanftest.log,690firmware | 164OK/0FAILED |
| 1.1.8 | xts695-hello.log,690firmware | PASS |
| 1.3.1 | guarded flash708,subsequent NSH | PASS |
| 1.3.3 | xts705/706/707,700firmware | w/h/b PASS,209256bytes each |
| 1.3.2 | xts703-fstest.log,700firmware | PARTIAL:10loops20OK/0FAILED, but fill logs ENOMEM; not counted |

Published BUILD_KERNEL scheduler suite contains9pthread cases;7task cases
are upstream BUILD_FLAT-only. ostest remains required but build-blocked by
privileged spinlock operations in a user ELF; no cases were edited or removed.
Kernel-mm TESTING_MM also currently depends on BUILD_FLAT.

## Fixes and commits

- Apps c7211d0fc: add existing common source to two CMake MM helper targets.
- N cee97921549: failed pthread address-environment cleanup, avoiding NULL group.
- N 5a622b5838d: kernel stack ABI alignment, removing unnecessary16KiB alignment.
- N 868da6f97f7: standard core/ostest profiles,16KiB standard test stacks.
- N 8fd463ef26a: restore CLIC return privilege from edited signal xstatus.
- N e9c4b1d2194: free libc task caches from their owning group heap.
- N bd3ba790bb9: common/RAM profiles,NAME_MAX255,remove temporary sched verbosity.

Previous unrelated WIP preserved, not included in these commits. Candidate
C++ profile currently uncommitted; build712 is next, not presumed successful.

## Failure-to-fix chain

- 681/682: scheduler case09 fatal;683/685 exposes ENOMEM with safe cleanup;
  686/688 passes9/9 after kernel-stack alignment fix;689 memory8/8.
- 690/692: syscall close03 filename too long, then SIGALRM trampoline
  instruction page fault. 696/698: signal casePASS,77/83 because6filename
  cases fail, then libc cache teardown mm_heapmember assertion.
- 700/702: NAME_MAX255 gives83/83, but same exit assertion. 704/709:
  group-aware libc cache free gives83/83 and normal NSH return.
- 703: filesystem's intended fill reaches small internal kernel heap;
  CRC/deletion10loops complete20/0, but capacity ERROR logs remain unresolved.

All build commands use absolute paths:
`S31_DEMO_PROFILE=<profile> bash D/build-demo.sh D/buildNNN-<profile>.sha256 > D/logs/buildNNN-<profile>.log 2>&1`.
Here D=/home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress.
Profiles/builds:683/686 xts-core;690/696 xts-common;700/704 xts-memory.
The profile variable includes full `demo-rmt-` prefix in every execution.
Flash commands: `bash D/flash-demo-pair.sh D/buildNNN-<profile>.sha256`,
redirected to flash684/687/691/697/701/708 respective full logs; all exited0.
Standard commands and parameters are explicitly printed as XTS_COMMAND in
each serial log. Host transport P is the existing reference .venv-nuttx Python,
invoked with `-u D/xts-run-standard.py <case> [--no-boot] [--width h|b]`.
No test body or predicate modified.

709 first launch did not execute: automatic permission reviewer timed out.
One system-permitted retry succeeded. This was not a device failure.
692's target exited before transport recognized page faults; its sole host
collector PID1320012 was identified then terminated. Full failure retained.
Transport now detects page faults and drains5seconds of crash output.

## Safety / next actions

Board holds704, latest710/711 done, no serial owner. Preserve it until next
validated flash; firmware704-xts-memory.tar.gz contains kernel/AppFS/config
and debug syscall ELF. Earlier678/683/686/690/696/700 also archived.
Original recovery demo657 remains in progress654-662.tar.gz.
Continue C++ and other published tests; no reference edits/downloads, efuses,
router/proxy changes or raw writes beyond authorized0x2000/0x200000 images.
No overall Wi-Fi/HE completion claimed. bgn demo previously usable; HE still
unaccepted. BLE/Zigbee last, not removed from scope.

Style checks: git diff --check and direct nxstyle task_uninitinfo.c passed.
Generic checkpatch invocation was not a valid PASS: missing toolchain PATH,
pre-existing long line and range-argument script issues were observed.
