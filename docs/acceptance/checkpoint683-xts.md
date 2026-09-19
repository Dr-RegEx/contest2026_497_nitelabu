# xTS-first checkpoint, 2026-09-14

Priority: published applicable tests and usable demo; no perfection campaign.

## Evidence and exact recent commands

Working root: `/home/regex/work/esp32s31-openvela`.
In the commands below D denotes `backups/2026-09-10-scan-stress` under that root,
and P denotes `s31-reference/.venv-nuttx/bin/python` (absolute paths used).

- `P -u D/xts-run-standard.py mm --no-boot > D/logs/xts680-mm.log 2>&1`:
  PASS, all 8 unchanged standard cases; firmware678.
- `P -u D/xts-run-standard.py sched --no-boot > D/logs/xts681-sched.log 2>&1`:
  FAIL, first 8 pass, case09 fatal, mtval=0xc4.
- `P -u D/xts-run-standard.py sched > D/logs/xts682-sched-reboot.log 2>&1`:
  same failure after fresh reset, exit1. Not a consequence of prior mm execution.
- `S31_DEMO_PROFILE=demo-rmt-xts-core bash D/build-demo.sh D/build683-xts-core.sha256 > D/logs/build683-xts-core.log 2>&1`:
  exit0, both artifact hashes verified. No source-test edits or dependency fetch.
- `bash D/flash-demo-pair.sh D/build683-xts-core.sha256 > D/logs/flash684-xts-core.log 2>&1`:
  started; verdict must be read from terminal/log, not presumed here.

## Current changes

Apps commit c7211d0fc fixes missing existing common source in two CMake memory
performance helper targets. NuttX HEAD f64e492fe5a, previous WIP preserved.
New xTS profiles remain uncommitted pending target validation.

NuttX addrenv_leave now handles a failed, never-activated pthread whose group
binding was cleared by nx_pthread_create error cleanup. It releases the
reference recorded by addrenv_join in addrenv_curr, and tolerates NULL when no
reference was acquired. Normal group-owned exit remains unchanged. This is a
candidate standard-test blocker fix, not yet a target PASS.
Test-profile scheduler error logging enabled to expose the original creation
failure hidden by the fatal cleanup path.

Original firmware678 preserved as firmware678-xts-core.tar.gz,
SHA256 e93c1ebb2fa0ade0242105dbfe2da8aff090f9d1bfbe2ed5445aa884c79f0e80.
Recovery demo657 remains in progress654-662.tar.gz.

## Next standard prerequisites

Syscall profile still needs documented NET_LOCAL, FAT_LFN and IOB settings;
no syscall PASS claimed. Source cleanup uses /tmp/CM_syscall_testdir and a
small set of pseudo-FS FIFO/symlink paths, not persistent Flash formatting.
Kernel-ostest remains build-blocked by privileged spinlock use from user ELF.
Common1.1.6 TESTING_MM currently depends on BUILD_FLAT; do not silently remove
that requirement or count it passed by cmocka_mm_test.
