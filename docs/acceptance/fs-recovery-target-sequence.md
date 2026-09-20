# Filesystem recovery cases — candidate973, BUILD ONLY

Use only after longrun864 is complete and its evidence reviewed, Flash946/947
qualification and both pre-test backups are preserved. Never use production
/apps or forceformat during a recovery check. No network or external test
fixture is required for the software reset/crash alternatives.

973 is a separate FLAT filesystem image with the same fixed1MiB /dev/xtsflash
partition, PSRAM-stack Flash dispatcher and LittleFS. It enables
BOARD_RESET_ON_ASSERT=2 for automatic recovery from test03's deliberate crash.
TESTING_TESTCASES_STACKSIZE=32768 satisfies test04's explicit prerequisite;
this is larger than test03's listed8192 and must be recorded. Existing950 stays
available for unchanged short/performance tests. Flash only kernel0x2000 with
the matching973 receipt; retain all scratch data and do not auto-mount/format.

## 5.1.18 CRC after reboot and crash

Use a fresh /data/s18r for reboot and /data/s18c for crash. Do not reuse a
previous failed run or delete it silently. Verify current /data is scratch
LittleFS, no other filesystem workload/kvdbd is running, and capacity is
sufficient. Each RV32 record is1036bytes. The automatic trigger occurs after
402..901 attempted records, up to933436bytes plus filesystem overhead. Preserve
actual `df` output; do not reduce original counts to fit. Save existing KVDB
results first, since available space is shared.

For each mode, create its directory once, then invoke the original program:

```text
mkdir /data/s18r
vela_fs_stability_test03 mode=reboot /data/s18r
```

Retain write-progress lines, the intentional reboot message, reset/boot log and
new NSH prompt. The host must not reset the board to substitute for a missing
automatic reset. On the new boot, recreate only the volatile mountpoint if it
is absent, and mount the existing volume without any formatting option:

```text
mkdir /data
mount -t littlefs /dev/xtsflash /data
mount
ls -l /data/s18r
vela_fs_stability_test03 mode=reboot /data/s18r
```

Run `mkdir /data` only if absent. Before the final command, retain the existing
`stability_test03_file` size: it must be nonzero and a multiple of1036 for the
original reader to cover its entire contents. A missing/empty/partial file is
not replaced by a new run. The checker logs `file is exit !`, `check crc OK !`
and `TEST PASS !` and then deletes its test file. Also reject diagnostics such
as bad CRC/length, write/read errors or unexpected reset. Source test03 itself
is unchanged and ignores some I/O return values; keep this limitation explicit.

Repeat exactly the same two-stage sequence with `mode=crash` and /data/s18c.
Keep the intentional trap evidence separate from unexpected faults. Both modes
must complete before reporting the whole5.1.18 case. Never turn a timed-out
crash into a PASS via a host reset. No extra repeat count is added.

## 5.1.19 File integrity after controlled reset

Create fresh /data/s19 and run:

```text
mkdir /data/s19
vela_fs_stability_test04 /data/s19
```

Follow the original10–20second window, for example a recorded15seconds from
invocation, using the board RESET or explicitly logged UART hard reset. Observe
both writing threads before resetting. They write4096bytes every200ms; file1
fsyncs each write and file2 does not. Leave the original30second automatic
crash thread unchanged; it is not the published10–20second manual reset step.
Do not let it silently replace that step or overfill the1MiB partition.

After reboot, remount existing LittleFS without formatting, retain `ls -l
/data/s19` and both file sizes, then rerun the same command. A missing or empty
file does not establish recovery even if a checker prints PASS. Retain original
write-progress records, reset timing, recovered sizes, both checks and result.
The checker deletes both files after success, so capture evidence before it.

### Necessary test-source correction in973

The original test04 reader opens file1 even when asked to check file2, writes
one byte beyond a512-byte allocation after a full read, and uses a moving
strlen-based loop that skips data. Candidate973 fixes only that reader:
open the supplied filename, check allocation/read errors, distinguish EOF,
and validate every byte using the returned length without a terminator write.
Writer workloads, delays, command syntax and result labels are unchanged.

This is explicitly a PATCHED TEST SOURCE; do not describe its future result as
an unmodified upstream-test run. Original source and patch are archived with973.
A host ASan/UBSan reproduction confirmed the original heap overflow; the fixed
reader accepts a healthy1024-byte file and rejects corruption at byte900 in the
second file and a read error. This is host regression evidence, not board or
persistence acceptance. No board test has been run for973.

## Prepared1032 transport

`run1032-fs-recovery.py --case 18-reboot|18-crash|19 --mount-evidence
<current1032-boot-log> --output <new-directory>` runs one selected original
mode on the existing3MiB LittleFS. No initial reset, mount or format is used:
the pre-existing mount must survive opening the tty. Each mode creates its
fresh s18r/s18c/s19 directory and refuses previous leftovers. Test03 must
reboot itself; a timeout never triggers a host reset. Test04 observes both
writers and resets at15seconds, refusing a missed10–20second window.

After the expected reset the same script verifies the3MiB boot marker, mounts
without formatting, records recovered file sizes, and executes the original
checker. Empty/missing files and partial1036-byte records are rejected before
test03 can delete or falsely pass them. Test04's reader patch remains disclosed.
The source checker itself removes its test files; full evidence is captured
before that action. The script does not delete directories or unrelated data.

`uart.log` retains the whole run including deliberate crash diagnostics.
`mount-evidence.log` retains just the final boot's receipt/marker/mount segment,
so it can be passed to the next mode without falsely presenting a pre-reset
mount as current. Both18 modes must pass to report all of5.1.18. Help/AST checks
passed offline only; no UART or build performed by this script's author.
