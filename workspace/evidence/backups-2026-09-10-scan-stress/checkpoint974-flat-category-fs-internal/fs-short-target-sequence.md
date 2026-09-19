# Original filesystem target sequence: source/argument reconciliation

Prepared September16, TARGET NOT RUN. Candidate950 contains the original
stress/stability commands and Flash PSRAM-stack dispatcher. Use only the
isolated 1MiB scratch LittleFS after946 and947 finish and their evidence is
saved. Preserve KVDB evidence before any fresh formatting. Never use `/apps`.

The published xTS page and local test source differ in some command syntax and
success strings. Execute the original local programs with their actual options,
record source revision and exact invocation, and disclose these differences.
Do not patch their result strings or treat ignored arguments as applied.

| Case | Actual command | Evidence / scope |
|---|---|---|
| 5.1.9 | `vela_fs_multi_thread_read_test -p /data -l 1000 -t 3` | Three readers, 1000 ABC entries each; actual success string `TEST PASS!`, published `TEST PASSED`. |
| 5.1.8 | `vela_fs_multi_thread_write_test -d /data -l 1000` | Three writers, expected file size3000 bytes; actual `TEST PASS!`. |
| 5.1.6 | `vela_fs_stress_read_and_write_loops_test /data` | Original positional syntax and default10 loops; expect `TEST PASSED`. |
| 5.1.5 | `vela_fs_stress_multi_thread_file_operate_test -d /data` | Actual getopt syntax, original default thread count retained; expect `TEST PASSED`. |
| 5.1.3 | `vela_fs_stress_loop_create_delete_file_test /data` | Original100 iterations; each write uses free heap/10. Use disclosed internal-SRAM candidate974; PSRAM-heap candidate950 still exceeds capacity. See memory-profile note below. |
| 5.1.10 | `vela_fs_multi_thread_read_write_test -d /data` | Original default thread/data/iteration settings retained; actual `TEST PASS`. |
| 5.1.7 | `vela_fs_random_read_and_write_test writeCount=1000 readCount=1000 mountPath=/data` | Both count1000 and both timing reports required. Source creates800000 bytes before timing; fresh space required. |

The read/write tests use the same `multi_pthread_testfile` and append on open.
Confirm it is absent before each run and removed afterward; do not reuse a
partial file from a failed run. Preserve failures before removing leftovers.
For every case retain full logs, reject error diagnostics even if a later PASS
line appears, and record mount/free-space state. A shell return alone is not
acceptance. Original random-read/write main returns0 even after a subtest error.

## Capacity-sensitive cases

- 5.1.2 write_speed defaults to1000 writes of1024 bytes:1024000 bytes, leaving
  only24576 bytes of the raw1MiB for filesystem metadata. Check usable capacity
  before execution; do not silently lower the count or report expected ENOSPC
  as a driver defect. The raw1MiB size is not the free file capacity.
- 5.1.11 performance_test accepts explicit block/count/mode. Record the chosen
  resource-sized workload and run modes1,2,3,4 in that order against the same
  file. The published case allows configurable sizes. Preserve measured speed,
  do not invent a throughput threshold, and check negative/error output even
  though main returns0.
- 5.1.12 dd likewise needs disclosed file size below actual free capacity.
- 5.1.16 fragmentation uses integer MiB `-s` and defaults to100 large files.
  Even one1MiB file cannot fit a1MiB formatted partition. This candidate cannot
  provide this Flash workload merely by choosing `-s 1`; leave it pending or
  separately qualify an explicitly identified larger disposable filesystem.
- 5.1.4 maximum-name and5.1.1 full-partition workloads run last on disposable
  storage. Save all earlier evidence first; leftovers can require reformat.
- 5.1.15 remains original fstest1000, and5.1.17 remains the declared timed
  stability workload. Short cases or RAM results do not replace these.

No workload counts, success checks, or original test source were changed by
this preparation. Reset/crash cases follow separately on persistent storage;
physical power-cut cases remain reserved for September17.

## Prepared one-case runner972

`xts-fs-short.py` supports 5.1.5, 5.1.6, 5.1.8, 5.1.9 and 5.1.10 once
per invocation. It reuses `xts-kvdb-suite.py` for existing raw115200 UART access
and explicit current-boot scratch mount evidence. Use after longrun completion,
Flash qualification and a recorded mount on candidate950. No reset, mount,
format, deletion, provisioning or retry is performed by this runner.

Example (the mount evidence must be an actual transcript for this boot):

```sh
python3 -u backups/2026-09-10-scan-stress/xts-fs-short.py \
  --receipt backups/2026-09-10-scan-stress/build950-flat-category-fs-perf.sha256 \
  --mount-evidence /path/to/current-boot-scratch-mount.log --case 5.1.6
```

Retain stdout/stderr as the raw transcript. Existing case directories are
refused, not deleted: /data/s05, s06, s08, s09 or s10. These short paths keep
`test.h`'s fixed20-byte setup buffer within bounds after `/testDir` is appended.
They identify the same scratch LittleFS while avoiding existing KVDB files.
The runner refuses a running KVDB daemon, other FS workloads and nested mounts.

5.1.5 can log `write file fail !` without setting its test flag; such output
must be rejected despite a later PASS. 5.1.6 can internally lower its iteration
count when free space is small; require actual read/write iterations0 through9.
5.1.10 retains five threads,8192 integers each,1000 iterations as initialized
by source (its usage text says10000). Completion requires each thread's final
iteration output plus original PASS and absence of error diagnostics. A host
1800s deadline bounds waiting only; it does not shorten the board workload.

5.1.3 additionally selects each file's write size as `mallinfo().fordblks/10`.
950 uses a PSRAM user heap on the16MiB board, normally yielding roughly1.6MiB,
which exceeds the entire1MiB scratch partition. Do not reduce original rounds,
consume memory artificially, or classify this expected capacity failure as a
Flash driver defect. Qualification needs a separately chosen adequate disposable
filesystem or a documented memory profile; it remains pending.

972 is host preparation only; no new category PASS or board operation.

## Internal SRAM configuration974 for original5.1.3

974 inherits the scratch filesystem image but disables PSRAM user-heap addition
and selects MM_REGIONS=1. RAM_SIZE remains524288; original free-heap/10 thus
produces files below approximately52KiB in the existing1MiB LittleFS. No memory
is consumed artificially and the original algorithm/100 rounds are unchanged.
This is a disclosed alternate memory configuration: it does not qualify the
same workload with a16MiB PSRAM user heap. No Flash partition was enlarged.

After the same Flash/mount prerequisites, use the updated runner:

```sh
python3 -u backups/2026-09-10-scan-stress/xts-fs-short.py \
  --receipt backups/2026-09-10-scan-stress/build974-flat-category-fs-internal.sha256 \
  --mount-evidence /path/to/current-boot-scratch-mount.log --case 5.1.3
```

The runner refuses a PSRAM heap or mismatched memory size, requires fresh
/data/s03, and checks original100 creation iterations plus100 create/write/remove
success records and no failure diagnostic. Preserve the original `ret = ...`
file-size logs and actual free-space state. The source only rejects write==-1,
not all possible short writes; the original test's coverage limit remains
explicit. Other profiles and old950/972 artifacts are preserved.

974 links the separately documented test04 correction because it shares the
current tests tree; its intended5.1.3 program is unchanged. 973 remains the
specific crash-reset candidate. This preparation is not a target PASS.
