# KVDB category candidate 895

Full offline build PASS; TARGET NOT RUN. Kernel450992 bytes, separate output
esp32s31-xts-flat-category-kv. Original cmocka_kv_test, kvdbd and
vela_kvdb_stability_test02 entry points verified in System.map. Server mode
and temporary storage enable all30 original cmocka entries. Local sockets
and UnQLite use existing source; the existing apps/testing/testsuites CMake
already registers the cmocka suite. Only tests/testcases/kvtest needed its
parent CMake entry for the original stability app. No original test bodies
changed. BUILD_FLAT local-only network uses NETDEV_LATEINIT, with no radio.

This profile registers /dev/xtsflash at fixed0xc00000..0xcfffff, same as865,
without mounting or writing at boot. LittleFS is available for an explicitly
mounted isolated /data; /data/persist.db and /tmp/db are KVDB paths. A guarded
blank scratch backup is required before first formatting. Run the common865
raw block test before formatting this partition for category tests. Do not
run any raw block test once it holds useful filesystem evidence without
explicitly preserving that evidence. No format/flash/reset was performed.

For actual KVDB tests, first mount the disposable persistent /data and RAM
/tmp, then start kvdbd. The original stability case requires deleting only
this test persist.db and rebooting; remount this same /data after reboot,
restart kvdbd, then execute vela_kvdb_stability_test02 10. Expected output:
TEST PASSED! (published approximate12minutes). cmocka_kv_test must report its
actual full executed count; configuration alone is not acceptance.

Build893 detected duplicate cmocka registration; new duplicate integration
was removed in favor of the existing app tree. Build894 exposed missing
riscv_netinitialize for local-socket-only config; NETDEV_LATEINIT matches
the established board strategy. Build895 is the successful result.

September16 update: candidate948 replaces895 for target execution with the PSRAM-stack fix described in checkpoint946-flash-psram-stack.md. Use build948-flat-category-kv.sha256 only after successful build receipt and archive verification. The old895 receipt points to its frozen image and must not be supplied to the active-output flash helper. Historical commands above retain the original preparation record.
