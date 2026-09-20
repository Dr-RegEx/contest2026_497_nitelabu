# 1174: Original random-write workload review

Case 5.1.7 remains NOT ACCEPTED for random-write performance.

Local original source: openvela-dev/tests/testcases/vela_fs_test/fs/stress/random_read_and_write_test.c.
setup() executes DATA_LEN=200 iterations; each fwrite writes DATA_LEN records.
Each RV32 record contains int number and char str[16] (20 bytes), so the file
contains 200*200*20 = 800000 bytes, not merely 4000 bytes. Every setup iteration
also invokes fsync. Timed randomWriteTest seeks within the first 200 records
and writes one 20-byte record per iteration. It does not time setup.

Existing1094 measured 1000 reads in0.55s and10 writes in43.21s without IO errors.
The observed write cost is not established to satisfy the published reasonable
millisecond-level expectation. Count adjustment is explicitly allowed, but
changing DATA_LEN, deleting the outer setup loop, dropping required fsync, or
switching the result to RAM would not establish this Flash filesystem result.

This source inspection explains actual workload, not a proven root cause of
write latency. No source, test count, or acceptance threshold changed. Preserve
original evidence and prioritize remaining original cases and fixture time.

Night review while1181 runs (source inspection only): local LittleFS
`littlefs/lfs.c` lfs_file_rawseek calls lfs_file_flush before changing the
position of a writing file. lfs_file_flush copies the unchanged suffix from
the write position to the old file size (lines3278ff,3676ff). NuttX fseeko
also drains the stdio write buffer before lseek; lfs_vfs.c forwards the seek
to LittleFS. Thus an original20-byte overwrite near the beginning of this
800000-byte file can require copying nearly the entire suffix at the next
seek. A small write payload alone does not imply millisecond Flash work.
This is a concrete source-level explanation to investigate, not a measured
attribution of1094's43.21s or proof of a driver defect. Removing explicit
mount sync alone would not remove this seek/CTZ-copy path. No filesystem,
source, workload, or acceptance result changed; keep current1181 uninterrupted.
