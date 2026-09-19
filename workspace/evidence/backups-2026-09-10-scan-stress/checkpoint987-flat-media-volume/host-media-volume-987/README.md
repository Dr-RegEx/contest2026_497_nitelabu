# 987 temporary WAV volume host review

Host only. No UART, firmware build, or physical Flash accesses by this review.

## LittleFS original-resource capacity

`lfs-capacity.c` links the existing unmodified NuttX checkout's
`fs/littlefs/littlefs/{lfs.c,lfs_util.c}` and runs against a host-memory array.
Geometry is 4352 x 4096 bytes = 17 MiB, read/prog/cache 256 bytes (MTD64 x
NuttX default factor4), lookahead256, block_cycles200 and name_max32.
The backing callbacks enforce erase-before-program and bounds.

The intact original 17,473,937-byte WAV is written once, closed, unmounted,
remounted, and read completely into `/tmp/lfs-capacity-987-readback.wav`.
Both SHA256 values match the original resource manifest. LittleFS uses
4277/4352 blocks, leaving75 blocks (307,200 bytes). ASan/UBSan reported no errors.
This verifies capacity for this resource, not physical media reliability,
combined-MTD implementation, transfer throughput, player heap margin or playback.

Build command:

```
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DLFS_NO_DEBUG -Iopenvela-dev/nuttx/fs/littlefs/littlefs backups/2026-09-10-scan-stress/host-media-volume-987/lfs-capacity.c openvela-dev/nuttx/fs/littlefs/littlefs/lfs.c openvela-dev/nuttx/fs/littlefs/littlefs/lfs_util.c -o /tmp/lfs-capacity-987
/tmp/lfs-capacity-987 'openvela-dev/docs/zh-cn/test_dev_guide/mediatool测试资源和测试步骤/测试资源/音视频测试资源文件/audio_file.wav' /tmp/lfs-capacity-987-readback.wav
```

## Actual combined-wrapper regression

`extract-wrapper.py` copies the actual complete `esp32s31_xts_media_volume.c`
with only includes removed, and copies actual MTD structs/dispatch macros from
`include/nuttx/mtd/mtd.h`. `wrapper-check.c` supplies host allocator, hardware,
registration and leaf-MTD mocks. Ioctl identifiers are distinct host constants;
no physical ioctl is performed. Both byte-write and block paths are covered.

Final ASan/UBSan checks PASS: byte/block read and write spanning14MiB, correct
local offsets and buffer advancement, last unit and empty end request; full
preflight rejection of negative offset, overflowing counts and beyond-end
requests before any child access; NULL buffer; short/negative/oversized child
return handling and successful-prefix counts. Erase split/tail/range and child
error handling pass. RAM erase must return0, Flash erase must return the exact
requested count: zero/short/oversized Flash results and RAM positive results
are rejected. GEOMETRY returns only17MiB; BULKERASE never reaches a child.
Actual initialization only allocates/initializes RAM, creates fixed Flash
partition0xd00000+0x300000, checks geometry and registers/dev/wavvol. Mocks
confirm no child read/write/erase and inert repeated initialization.

```
python3 backups/2026-09-10-scan-stress/host-media-volume-987/extract-wrapper.py
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wno-unused-parameter backups/2026-09-10-scan-stress/host-media-volume-987/wrapper-check.c -o /tmp/wrapper-check-987
/tmp/wrapper-check-987
```

The retained compiler stderr includes signedness warnings in bounded offset
expressions and a host assertion. No sanitizer errors. This is native-host
logic validation, not an RV32 ABI run, allocator capacity proof, real PSRAM
contiguity check, LPWORK timing verification, physical Flash test or playback.
Source hashes identify the frozen wrapper used for this final regression.
