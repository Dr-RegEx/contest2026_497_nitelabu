# Filesystem recovery973 — BUILD ONLY

419388-byte kernel built clean in a new output directory. Independent FLAT
profile, fixed1MiB scratch partition, same PSRAM-stack Flash dispatcher.
BOARD_RESET_ON_ASSERT=2 enables original test03 automatic crash recovery;
32KiB testcase stacks meet test04's prerequisite. No Wi-Fi/SMP, no automatic
mount/format. Longrun864 still owns the UART; no image has been flashed.

Corrected test04's reader only: requested filename, byte-length validation,
allocation/read error handling, no out-of-bounds NUL. Preserve original writer
workloads and timing. test03 source is unchanged. This image contains PATCHED
TEST SOURCE; report that fact in future results, never claim unmodified source.
Original test04, patch, current source and tests HEAD are included. The inherited
profile chain and other adaptation source are in source checkpoint970; add this
new defconfig and the test04 patch to reproduce973 with the supplied build helper.

Validation: F0 dependency lock PASS; clean build/config/symbol guards PASS;
NuttX/tests diff whitespace checks PASS; helper syntax PASS. Host ASan reproduced
the original512-byte overflow and the fixed checker accepted good data, rejected
second-file byte900 corruption and a read error. Host evidence is included;
check.c refers to the original workspace path and may need its include adjusted
when reproducing elsewhere. No hardware/persistence PASS is claimed.

Follow fs-recovery-target-sequence.md after longrun and Flash qualification.
Prior950 and its original test source image remain unchanged for comparison.
