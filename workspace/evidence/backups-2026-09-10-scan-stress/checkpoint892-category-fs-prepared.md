# Filesystem category candidate 892

Full offline build PASS; TARGET NOT RUN. Kernel383596 bytes. Receipt:
build892-flat-category-fs.sha256. The separate xts-flat-category-fs profile
inherits xts-flat-rtc and enables the original ROMFS/FAT UTF8 examples,
FAT formatting, filesystem stress/stability apps and fstest. Original case
bodies and assertions are unchanged. Testcase stacks are32768 bytes; this
FLAT profile does not use TLS_ALIGNED. The build verifies selected configs
and ROMFS/FATUTF8/stability/performance entry points in System.map.

Mount disposable RAM-backed FAT/TMPFS volumes explicitly for initial tests.
This firmware has no test Flash partition; it cannot establish persistent
storage, reset/power-loss or Flash performance acceptance. Production /apps
must not be used for fill/delete/format workloads. Candidate865 still owns
its separate guarded raw Flash test. A later isolated persistent filesystem
candidate is required for those category cases. KVDB is not yet enabled.

Original fstest5.1.15 requires1000 loops; this is not replaced by earlier
common10-loop evidence. Schedule its actual resource size/runtime explicitly
so September17 remains available for the physical fixture session.
