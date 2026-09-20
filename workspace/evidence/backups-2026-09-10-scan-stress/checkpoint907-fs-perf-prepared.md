# Filesystem throughput candidate907

Full offline build PASS; TARGET NOT RUN. Kernel419012 bytes. Independent
xts-flat-category-fs-perf output inherits905 without replacing its artifacts.
Adds original5.1.12 dd prerequisites (NSH_CMDOPT_DD_STATS, DEV_NULL, DEV_ZERO)
and5.1.11 TESTING_TESTCASES_PRIORITY=255. Original performance_test itself
also sets255. Tests and original workload counts are unchanged.

The board still only registers isolated /dev/xtsflash without writing at boot.
Existing double backup and original865 raw Flash qualification must precede
formatting. Use the same dedicated /data filesystem after qualification.
Suggested disclosed size for1MiB scratch: dd bs=4096 count=64 (256KiB);
performance_test -d /data -b4096 -c64 -m1, then modes2/3/4 on its own file.
Run commands separately, retain errors and measured read/write rates. The
published throughput cases defer threshold assessment to openvela community;
measured rates alone are not an invented threshold PASS.

5.1.13 raw-device dd is NOT automatically enabled by an MTD node: a suitable
block/character mapping is still needed. Do not issue dd directly to the MTD
node or write a mounted filesystem through a raw path. No target commands
were issued during longrun864. Source diff whitespace check passed.

September16 update: candidate950 replaces907 for target execution with the PSRAM-stack fix described in checkpoint946-flash-psram-stack.md. Use build950-flat-category-fs-perf.sha256 only after successful build receipt and archive verification. The old907 receipt points to its frozen image and must not be supplied to the active-output flash helper. Historical commands above retain the original preparation record.
