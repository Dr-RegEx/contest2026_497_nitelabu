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
- 5.1.16 correction after full source inspection: integer-MiB large writes
  intentionally stop successfully on ENOSPC/EFBIG, so1MiB capacity alone is not
  a deterministic blocker. The default1000 outer rounds each perform100 large
  writes/deletes with1s sleep, requiring more than28h even before I/O. Also,
  file_no is not reset between outer rounds, so only the first round creates
  the requested small files. Keep these original-source and scheduling limits
  explicit; do not shorten hidden loop parameters and claim full original coverage.
- 5.1.4 maximum-name executed on976 and FAILED (path traversal/chdir failure preserved in xts1533); 5.1.1 full-partition workload remains last on disposable
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
  --receipt backups/2026-09-10-scan-stress/build976-flat-category-fs-perf.sha256 \
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

## Published performance measurements5.1.11 and5.1.12

Use950 (original testcases priority255 and DD statistics), existing scratch
LittleFS and fresh /data/s11 or /data/s12 directories. Record actual free capacity
first. The published cases explicitly allow chosen blocksize/count and say the
manufacturer records rates for community acceptance. Planned workload4096x64
is262144bytes; disclose it and retain all errors, byte counts, time and rates.
Do not add extra rounds or a private speed threshold.

```text
mkdir /data/s11
performance_test -d /data/s11 -b 4096 -c 64 -m 1
performance_test -d /data/s11 -b 4096 -c 64 -m 2
performance_test -d /data/s11 -b 4096 -c 64 -m 3
performance_test -d /data/s11 -b 4096 -c 64 -m 4
ls -l /data/s11
```

Keep all four modes in order against the same performance_test file. Expected
labels are `write speed is`, `read speed is`, `random write speed is`, and
`random read speed is`. Values are KiB/s (1024-byte divisor), despite generic
source wording. The file remains for evidence; do not silently truncate an old
run. Check262144-byte size and reject error diagnostics even if a speed follows.
Zero, NaN or infinite rates need timing/allocation review rather than a PASS;
source uses clock() and does not include final close in its timed region. Keep
that measurement definition unchanged.

After saving those results, and only if enough free space remains, use another
fresh directory for the original dd sequence:

```text
mkdir /data/s12
dd if=/dev/zero of=/data/s12/payload bs=4096 count=64
dd if=/data/s12/payload of=/dev/null bs=4096 count=64
ls -l /data/s12
```

Both DD summaries must report262144bytes copied and actual elapsed microseconds
and KB/s. Preserve the file and log. If both payloads would crowd the volume,
archive evidence before any separately recorded cleanup; do not auto-format or
erase existing KVDB/recovery files. Results are measured rates pending community
acceptance, not an invented numerical PASS rule. These commands are prepared,
not executed on the board.

## Maximum-name and fill-volume preparation975

5.1.4 is now selectable with `xts-fs-short.py --case 5.1.4`, using974 (or a
verified matching LittleFS configuration), the same mount evidence and fresh
/data/s04. The original application creates a boundary path below its testDir,
then removes that subtree. Current NAME_MAX32, PATH_MAX256 and LittleFS name32
match. `new file length N` is normal progress even though source uses LOG_ERR.
Require original max-name/path output and result, not merely a shell return.
The runner does not implement the published FAT exception or automatic format;
it is specifically the prepared LittleFS case.

5.1.1 remains an explicitly last, manually supervised original command, after
all other storage evidence is retained and kvdbd/other writers are stopped:

```text
mkdir /data/s01
vela_fs_stress_write_full_file /data/s01
ls -l /data/s01
df
```

Use a new /data/s01. The program appends512-byte records until fwrite returns0;
LOOP_CNT2000 is a logging constant, not a2000-iteration limit. It normally logs
`1th round start`, then `create full file, takes ... seconds`, completion and
`TEST PASSED !`. It removes its own testDir recursively, not other directories,
but consuming free space affects the entire shared filesystem during the test.

The original does not record terminal errno or distinguish ENOSPC from other
write errors; disclose that coverage limitation. Do not require an invented
errno28 output, ignore an actual `Fail to open file!`, or claim this test proves
power-loss safety. Preserve any timeout/residual files; no automatic cleanup,
format or repeat. The current preparation has not run either case on target.

## Duration reconciliation for5.1.17

The published step is `vela_fs_stability_test01 -d DIR -t 1`, describing hours
and noting12h is usual. This tree's parser/help and comparison actually use
MINUTES (`test_time * 60`). A declared1h run therefore requires `-t 60`; a
chosen12h run requires `-t 720`. Preserve this source/document argument mapping
and actual board/host elapsed time. Never report literal `-t 1` as1h, or use
standby3.1.1 evidence as a filesystem-stability result. The original example's
1h scope and optional customary12h must not be silently conflated.

Use a fresh /data/s17 on scratch LittleFS; source constructs stab_test_dir
below it and removes any old directory at startup. The runtime uses one small
shared file and transient512-byte files, not an ever-growing multi-MiB payload.
Use corrected candidate976 for this timed case. Its necessary test01 patch
sets exit flags and joins every successfully created worker before reading
results, closing the shared stream and cleanup, including partial thread-create
failure. The old2second delay could race still-active workers. Workload, three
worker bodies and minute units are unchanged. This is PATCHED TEST SOURCE and
must be reported as such. Host real-thread regression reproduced the old race
and verified normal/error shutdown, not board stability.

For5.1.15, keep exact `fstest -m /data/fstest -n 1000` after creating a new
directory and preserving other storage results. Existing950 has NLOOPS100 but
the explicit-n1000 overrides it (NLOOPS is not zero/infinite). MAXFILE32768,
MAXNAME32 and MAXOPEN512 are compiled resource choices and must be disclosed.
Require FILLING/DELETING1..1000, final summary with FAILED0, no error/crash,
and save all evidence before the published final directory removal. Original
text warns some boards need two days; do not silently replace1000 with a smoke
count. No runtime estimate is available before the first hardware run.

## Current shared performance/stability candidate976

976 supersedes the active fs-perf output for future execution, with test01
worker-drain and test04 reader corrections. The other original short and
performance programs are unchanged. Old950 receipt now points to its verified
frozen checkpoint950 image; the old archive is preserved. When this document
mentions historical950 capability/configuration, use976 for the active profile
and its matching receipt unless explicitly restoring the archived image.

Planned published-example duration for5.1.17 is1h, expressed by the existing
minute-based program as `vela_fs_stability_test01 -d /data/s17 -t 60`.
Log this mapping and actual duration.12h is the original note's customary option,
not silently added to the example. Execution and duration remain unmeasured.

## Remaining original speed cases: preparation977

Both programs are linked in candidate976. No target execution has occurred.
Stop other filesystem writers and retain previous evidence first. Inspect `df`
and `ls -l /data`; use fresh short directories on the existing scratch mount.
Do not automatically format or delete earlier data to make capacity available.

For5.1.7, execute these as one command line after creating /data/s07:

```text
mkdir /data/s07
vela_fs_random_read_and_write_test writeCount=1000 readCount=1000 mountPath=/data/s07
ls -l /data/s07
df
```

The published line breaks separate arguments, not interactive prompts. The
program defaults to10 if those explicit counts are omitted. Setup writes200
copies of200 records of20 bytes, totaling800000 bytes before timing, and calls
fsync200 times. Preserve enough actual free capacity plus metadata headroom;
random accesses then address only the first200 records. The directory must
exist because the program ignores chdir's result. Require both1000-count timing
reports, no error/skip output, and the final data listing. Successful shell exit
is insufficient. The source does not compare the read values against expected
records, and final buffered write/close are outside the reported write time.
Preserve these coverage limits. Report the measured times for original
millisecond-scale expectation review; do not invent a numeric threshold or
claim a source PASS marker (there is none).

For5.1.2, reconcile the published positional DIR with this source's getopt:

```text
mkdir /data/s02
vela_fs_stress_write_speed -d /data/s02
ls -l /data/s02
df
```

A positional DIR is ignored by this version; `-d` is required to select the
fresh directory. Keep default1000 writes of1024 bytes. Require Write success
No.1 through No.1000, File size1000K and TEST PASSED without write/open errors.
The program uses append mode inside /data/s02/testDir and removes that subtree
at completion. Its fsync and fclose return values are unchecked; a success
marker alone does not establish power-loss durability. Its elapsed time also
includes1000 progress messages and filesystem activity, not pure device speed.
Only24576 raw bytes remain for filesystem overhead before any existing files;
usable capacity must be assessed on board. If insufficient, preserve the
blocked/failed result and arrange storage explicitly; do not silently use-c to
reduce the workload, expand the partition, or erase the KVDB evidence.

## Existing-volume mount after1013 (prepared helper, no target execution here)

After the sole UART owner flashes the receipt-checked976 image, use:

```
python mount-fs-scratch.py --receipt build976-flat-category-fs-perf.sha256 --output <new-mount-evidence-directory>
```

This helper validates the fixed973/974/976 image hashes and configurations,
checks UART ownership, performs one initial reset and verifies the isolated
scratch boot marker. It mounts `/dev/xtsflash` at `/data` without formatting,
then records mount, file listing, df and free. It does not delete KV files or
run any filesystem workload. On failure it stops without recovery/reformat.
The new `mount-evidence.log` is accepted by the existing short-case runner.
A new receipt/current-boot transcript is needed after every image change/reset.

Planned976 order is5.1.5,5.1.6,5.1.8,5.1.9,5.1.10,5.1.4, each invoked once
with `xts-fs-short.py --case <case> --receipt <976-receipt>
--mount-evidence <current-boot-log>`. Each case uses its separate existing
runner directory `/data/s05`,s06,s08,s09,s10,s04; an already-existing directory
is refused, not erased or automatically reused. Keep/data/persist.db and all
KV evidence. Check reported free capacity before the heavier5.1.10 workload;
insufficient space is a blocker to resolve with evidence preservation, not a
reason to delete the DB silently. This is execution order, not measured timing.

Use974 only for the disclosed internal-SRAM5.1.3 profile, with its matching
receipt and a new no-format mount transcript. Use973 similarly for the
separately documented recovery cases; those cases require their own deliberate
crash/recovery sequence and are not executed by this mount helper.
