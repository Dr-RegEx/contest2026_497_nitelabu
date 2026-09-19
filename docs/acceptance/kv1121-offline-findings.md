# KVDB stability: offline findings after 1119/1121

No UART, build, or locked source modification was performed. The original 1119 verdict is FAIL. Earlier 1100 text PASS remains rejected because commits failed.

## Confirmed

1. The framework opens UnQLite with `UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING` (`frameworks/system/utils/kvdb/unqlite.c:155`). A rollback journal consuming an extra database image is not the explanation for this profile. Do not disable journaling again as a supposed fix.
2. `IO error while writing dirty pages` originates specifically from `pager_write_dirty_pages` returning an error in commit phase 1. It is not the generic message for final fsync/truncate or lock failure. A seek failure during a page write also reaches this message.
3. The Unix VFS `unixWrite` loops correctly over positive short writes, advancing offset/pointer. Negative lseek/write results become `UNQLITE_IOERR` regardless of errno, including ENOSPC/EINTR/EIO. Zero-byte writes become UNQLITE_FULL. The diagnostic lastErrno exists internally but is lost from the displayed pager message.
4. `return -2` in 1119 is `SXERR_IO` / `UNQLITE_IOERR`; it is not POSIX `-ENOENT`. NuttX LittleFS explicitly maps its NOSPC to `-ENOSPC`, which would subsequently be collapsed by this UnQLite path.
5. The original stability case ignores `property_commit()` and cleanup deletion returns. `kvdbd` also ignores its periodic commit result; CONFIG_KVDB_COMMIT_INTERVAL is 5 seconds. This explains why many insert lines and repeated commit errors can coexist, and why the original text PASS alone is insufficient. No test source change is proposed.
6. 1113 archived state contained a 147456-byte database and two unrelated 262144-byte files. 1119 began with the old database absent; first observed dirty-page error followed key 1909. 1121 shows database size 389120 bytes and ample RAM (~16 MiB free). This is not evidence of heap exhaustion.
7. 1121 `df` reports 293 used blocks on a 256-block volume, wrapping free blocks to 4294967259. LittleFS raw traversal visits committed metadata CTZ chains, dirty open-file CTZ chains, and a writing branch; `lfs_fs_size_count` increments for every visit without unique-block deduplication. NuttX directly subtracts this count from block_count. Thus this display cannot prove actual allocation of 293 distinct blocks or disk corruption, and its wrapped free value must not be trusted.
8. Frozen 948's `littlefs_write` ELF does not call `lfs_file_sync`. Current source has a later optional sync-writes addition; attributing 948's error to that later implementation would be incorrect.

## Strong hypothesis, not yet proven

UnQLite's 4 KiB random page updates and LittleFS copy-on-write interact poorly on a 1 MiB volume. `lfs_file_flush` copies the unchanged tail after a modified position, while the last committed chain and current dirty/writing chains remain live. A 389120-byte logical database is already ~380 KiB; multiple live versions can approach/exceed the available volume even after removing 512 KiB of unrelated files. There is no UnQLite journal here, but that does not eliminate LittleFS copy-on-write peak space. The exact unique-block peak and first errno are still unobserved.

The alternative hypotheses (physical IO failure, interruption, invalid seek offset, corrupted pager offset) remain possible until the first underlying errno is captured. The ordinary <1 MiB offsets do not themselves support an off_t overflow theory. The VFS uses lseek+write, not pread/pwrite, with server operations serialized in its event loop; there is no current evidence of a shared-file-offset race.

## Minimal next diagnostic

Prepared, NOT APPLIED: `kv-next-errno-diagnostic.patch`. It touches only the compiled UnQLite amalgamation's `seekAndWrite` failure paths, reporting fd, requested offset/count, returned offset/byte count, and preserved errno. It preserves return values and errno and does not retry, roll back, suppress, or turn errors into success. CMake compiles `unqlite/unqlite.c`, so editing only `src/os_unix.c` would not reach this image.

After scope approval, use a separate diagnostic candidate and unchanged original case. The first failure line determines the next action: ENOSPC calls for measured copy-on-write/volume capacity remediation; EIO/EFAULT requires block/filesystem investigation; offset mismatch requires seek/pager tracing. Freeze the failed volume before any repair. No extra broad test suite or feature work is needed.
