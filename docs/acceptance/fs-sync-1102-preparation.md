# 1102 LittleFS explicit synchronous-write mount — SOURCE READY, NOT BUILT

1083 preserved evidence shows file1 with fsync237568bytes, file2 without
fsync0bytes after the original15-second reset. Empty file2 is not acceptance.

Minimal change only in `nuttx/fs/littlefs/lfs_vfs.c`: per-mount `sync_writes`
is enabled only by the exact mount-data string `sync`. After a positive
lfs_file_write return, call lfs_file_sync on that same file while holding the
existing filesystem mutex. Return the original byte count only on successful
sync; return the mapped sync error on failure. File position still reflects
the bytes accepted by the underlying write; it is not rolled back on commit
error. No recursive call to the locking VFS littlefs_sync wrapper.

Normal mounts are unchanged. No change to original test writers or their
counts,4096-byte blocks,200ms delay,15-second host reset, or30-second crash
thread. Existing test04 reader-safety patch disclosure remains required.
No upstream LittleFS, HAL or IDF edits. No format option is implied or accepted
as part of `sync`; combinations are deliberately not implemented.

The eventual explicit command, only with a newly built1102 image and the
existing3MiB filesystem unmounted, is:

```
mount -t littlefs -o sync /dev/xtsflash /data
```

No command has been executed here. Preserve1083 s19 and all1032 data; use a
fresh short test directory for the one later original test04 rerun. Do not
format, regenerate content or claim1083 passed. Current transport evidence
validators only recognize the old no-option mount; update the1102 runner
explicitly when scheduled, including sync mount again after controlled reset.

Semantics: a positively acknowledged write now commits its file data and
metadata through LittleFS before return, so the unsynced writer's completed
writes acquire real persistence. LittleFS ultimately invokes its backend sync;
the current direct ESP Flash writes are synchronous, and the VFS treats its
unsupported flush ioctl as already flushed. This is not a guarantee that an
in-flight write interrupted before return survives. Original writer throughput
and number of completed writes in15seconds can decrease. Exact retained byte
counts, nonempty both files and full-byte checks still require actual board
execution. It does not preserve bytes that the application never successfully
wrote, or make the original writer check ignored failures.

Necessary host regression in `host-littlefs-sync-1102/` extracts the actual
modified VFS write function, mocking lock/LittleFS calls. ASan/UBSan PASS for
default behavior, sync under the same lock, sync-error propagation, unchanged
write/seek error behavior, zero writes, and file-position semantics. This
checks the implementation boundary, not physical durability or full LittleFS
integration. No firmware build or UART/volume operation was performed.

## Frozen1102 build and prepared execution

Independent profile `xts-flat-category-fs-sync` inherits1032 large config.
Clean build PASS,419552bytes, SHA256
`54d06bdefc5837541d08fbf3e66500d918d7ea4ce1f9d9efd5de80b8ff4c5ded`.
Receipt `build1102-flat-category-fs-sync.sha256`; frozen artifact directory
`checkpoint1102-fs-sync/`. The image uses all current shared source changes,
including the existing inode NAME_MAX fix and test04 reader patch; it is not
claimed to differ from1032 only in LittleFS sync. Current nuttx/apps/tests
tracked diffs are archived along with exact relevant source and build outputs.
1032 frozen outputs and on-board data were not modified by this preparation.

Sole UART owner flashes with S31_FLAT_PROFILE=xts-flat-category-fs-sync and
the1102 receipt using flash-xts-flat.sh. Preserve a current boot transcript
including image hash, NSH and3MiB scratch marker. Then:

```
python mount1102-fs-sync.py --boot-evidence <current1102-boot-log> --output <fresh-mount-directory>
python run1102-fs-sync-recovery.py --case 19 --mount-evidence <fresh-mount-directory>/mount-evidence.log --output <fresh-test-directory>
```

Initial mount and post-reset mount both explicitly use `-o sync`; neither
formats or deletes data. The test uses fresh `/data/s19s`, preserving `/data/s19`.
Both transports retain the root's raw os.open/flock UART implementation, which
does not set DTR/RTS on open. Only the intended15-second reset asserts reset.
The pre-existing failed run stays failed; no target result for1102 exists yet.
Help, AST and shell syntax validation passed; no hardware operation performed.
