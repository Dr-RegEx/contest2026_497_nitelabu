# Internal-SRAM filesystem974 — BUILD ONLY

418236-byte clean build. Dedicated original5.1.3 configuration using internal
SRAM user heap, MM_REGIONS=1, RAM_SIZE524288. PSRAM user heap disabled; peripheral
initialization remains inherited. Same1MiB Flash scratch partition. No original
5.1.3 program/count/algorithm change, no artificial memory consumption. Qualifies
this memory configuration only, not the PSRAM-heap workload of950.

Source review confirms allocator span is internal SRAM and addregion is not
built for MM_REGIONS=1. NAME_MAX and LittleFS NAME_MAX both32. Pinned LittleFS
write completes the requested length or returns a negative error; it does not
return a positive partial result on ENOSPC. Original close-error checking limits
remain unchanged. A fresh short /data/s03 avoids old files and the test helper's
20-byte path buffer limit.

Updated runner requires the disclosed memory configuration and all100 original
create/write/remove iterations. Host syntax and missing-round rejection checks
passed. Build config/symbol guards, F0 dependency lock and source whitespace
checks passed. No UART, flash, mount or filesystem workload was run on target.

Source base is970 plus new defconfig; included test04 patch is linked because
this build shares the current tests tree, although intended5.1.3 is unchanged.
Use973 for automatic crash-reset tests;974 keeps the inherited assert policy.
See fs-short-target-sequence.md. All target results remain pending.
