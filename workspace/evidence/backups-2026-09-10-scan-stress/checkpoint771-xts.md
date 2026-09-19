# S31 xTS checkpoint — 2026-09-15

Verified common checklist coverage: **19/35 (54.3%)**, not whole-project
completion. Published original tests and usable hardware remain the priority.
No reference checkout changes, downloads, efuse writes or partition expansion.

## Results since checkpoint744

- NIST752: all ten 400000-bit streams computed, but partitioning result files
  failed creating RandomExcursionsVariant/data5.txt. Final report empty. Not PASS.
- WSL migration preserved Linux paths and repositories. USB was reattached as
  BUSID3-1. Native ttyUSB0 existed, but regex lacked dialout membership;755
  failed EACCES. Only this device's group was changed to regex, mode660, via
  existing WSL root access.755b NSH/free readout succeeded. Volatile old NIST
  files were already absent following an intervening reset.
- Crypto753 first real error: unresolved curve25519/generate_public in the
  kernel software crypto backend.754 enables its existing CRYPTO_RANDOM_POOL
  prerequisite; buildPASS, flash756 hashPASS.757 host parser did not recognize
  suite-name prefixes in cmocka summaries. After parser correction,759 PASS
  for all eight original apps: 3DES1, AES-CBC1, AES-CTR1, AES-XTS1, HMAC3,
  HASH4 (including original600KiB blocks), CRC32 4, ECDSA-P256keygen/sign/verify.
  These validate the **software backend**, not hardware accelerator acceptance.
- ostest758 links after building spinlock.c only for BUILD_FLAT, matching
  the unchanged original ostest_main call condition. Flash760 was blocked
  **before writing**: AppFS3206144bytes exceeds3145728. Its image mistakenly
  included old applications still in the reused bin directory.
- AppFS now uses CMake's current loadable-target manifest.762 buildPASS,
  AppFS1343488bytes. Old bin files preserved, disabled apps excluded. Two
  focused host tests761PASS.763 flash/hashPASS. This is a packaging regression
  check, not an additional xTS acceptance case.
- ostest764 reaches mutex/semaphore/pthread/message queue stages then exits
  early when SIGUSR1 is sent to the receiver. Missing remaining test phases:
  status0 alone is NOT PASS. Shared pending signals currently lose the chosen
  recipient when deferred during a syscall; candidate fix771 retains that
  recipient and cleans up addressed signals before the thread leaves its
  group.771 buildPASS; target validation pending. No original assertion edited.
- Filesystem heap765: reserves existing PSRAM0x50c00000..0x51000000 for4MiB
  tmpfs storage and excludes it from the12MiB process page pool. NOLOAD section
  plus linker assertions; no persistent filesystem modification.766 flashPASS.
- **fstest767 PASS**: unchanged `fstest -n 10 -m /tmp`,10loops,20OK/0FAILED,
  no ERROR/ENOMEM,64.948s. Original512-file/default-size workload unchanged.
  Adds common case1.3.2 to the verified count.
- Regression768 memory8/8PASS3.487s;769syscalls83/83PASS14.844s. Expected invalid
  socket probes print errors but each original assertion passes and teardown
  is clean. They are regressions, not extra checklist headings.
- NIST770 on765 is **running at this checkpoint**; no final RNG verdict yet.
  Do not reset/flash or open a second UART client until the process exits.

## Exact execution pattern

All paths below are under `/home/regex/work/esp32s31-openvela`.

```sh
S31_DEMO_PROFILE=demo-rmt-xts-ostest bash backups/2026-09-10-scan-stress/build-demo.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build758-xts-ostest.sha256 > backups/2026-09-10-scan-stress/logs/build758-xts-ostest.log 2>&1
```

The same command was used for762 (`demo-rmt-xts-ostest`),765
(`demo-rmt-xts-fsheap`),771 (`demo-rmt-xts-ostest`, receipt suffix
`xts-ostest-sigpending`). Each log records complete CMake configure/resetconfig/
build commands. Existing toolchain/IDF/HAL paths only; fully disconnected mode.

```sh
bash backups/2026-09-10-scan-stress/flash-demo-pair.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build765-xts-fsheap.sha256 > backups/2026-09-10-scan-stress/logs/flash766-xts-fsheap.log 2>&1
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-crypto.py > backups/2026-09-10-scan-stress/logs/xts759-crypto.log 2>&1
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-run-standard.py ostest > backups/2026-09-10-scan-stress/logs/xts764-ostest.log 2>&1
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-run-standard.py fstest > backups/2026-09-10-scan-stress/logs/xts767-fstest.log 2>&1
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-run-standard.py mm --no-boot > backups/2026-09-10-scan-stress/logs/xts768-mm-fsheap.log 2>&1
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-run-standard.py syscall --no-boot > backups/2026-09-10-scan-stress/logs/xts769-syscall-fsheap.log 2>&1
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-nist.py > backups/2026-09-10-scan-stress/logs/xts770-nist-fsheap.log 2>&1
```

Flash756/760/763 follow the same guarded command with receipts754/758/762.
Only0x2000 kernel and0x200000 AppFS are written;0x500000+ preserved.

## Commits and recovery

- apps579df39fd: ostest FLAT-only spinlock source condition; build758/762.
- NuttXe7a950e50d1: current-target AppFS packaging + original NIST template.
- NuttXf9effae1a2b: PSRAM filesystem heap isolation;767/768/769targetPASS.
- NuttXb621820cb1e: configurable FILE stream cap (default16) + NIST profile64.
- apps8bce52fb6: use existing local NIST checkout without fetching/repatching.

Firmware754(Crypto),762(ostest baseline),765(FS heap validated) archives retain
paired kernel/AppFS/config. Firmware765 additionally retains link map.
Earlier verified SHA256SUMS-xts730/750 and recovery bundles remain untouched.
Signal candidate, C++ unwind WIP, earlier Wi-Fi/mqueue/diagnostic WIP stay
uncommitted and must be preserved. No full directory replacement was used.

Next: finish770, validate771 against full original ostest, continue the cases
that need no external fixture. RNG and hardware crypto remain unaccepted;
RTC/FLAT-only cases, long-duration and external-instrument cases remain open.
