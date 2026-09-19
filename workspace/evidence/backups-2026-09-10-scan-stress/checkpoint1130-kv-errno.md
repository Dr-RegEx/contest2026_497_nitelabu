# KV 1130: reveal the first underlying commit IO error

DIAGNOSTIC ONLY. Original 1119 is FAIL; no guessed capacity or filesystem fix is included.

The actual compiled UnQLite amalgamation `external/unqlite/unqlite/unqlite.c` now reports failed lseek/write fd, offset, requested count, return value and errno in `seekAndWrite`. It preserves errno and all return/control behavior. Only a NuttX stdio declaration was added for the failure messages. Reference IDF/HAL and original tests are unchanged.

Distinct profile: `xts-flat-category-kv-errno1130`, including the original category-KV profile. Independent output: `openvela-dev/out/esp32s31-kv1130`.

Geometry and test configuration match frozen 948: xTS scratch offset 0xc00000, length 0x100000 (1 MiB); CONFIG_ESPRESSIF_STORAGE_MTD_OFFSET=0x180000 is a separate storage configuration, not the hard-coded xTS device offset, LittleFS factors/name/file limits, UnQLite options, KVDB persistent path `/data/persist.db`, periodic commit interval 5 seconds, and original stability application. Full selected KVDB/UnQLite/LittleFS/MTD configuration comparison found no differences.

Helpers `build1130-kv-errno.sh` and `flash1130-kv-errno.sh` exclusively whitelist this diagnostic profile and point to the independent output. Flash helper additionally requires the 1 MiB geometry, 5-second commit interval, original stability option, persistent path, and both compiled diagnostic strings before applying the existing backup/receipt/kernel-only checks. It was not executed by this agent.

Receipt: `build1130-kv-errno.sha256`.
Final build log: `logs/build1130-kv-errno-final.log`.
Initial compile log is retained as `logs/build1130-kv-errno.log`; its missing stdio declaration was corrected before the final build.

Final build exited zero. Both diagnostic strings are in the ELF, 1130 and frozen 948 SHA receipts verify, and shell syntax checks pass. The scoped source diff check passes with `core.whitespace=cr-at-eol`, honoring the amalgamation's existing CRLF line endings. Generated configuration and exact diagnostic patch are preserved in `checkpoint1130-kv-errno/`. Build resource released. BUILD PASS / TARGET NOT RUN.

Root owns UART. Run the unchanged original case and preserve the first `KV VFS ... failed` message, not just the generic pager message. ENOSPC remains a hypothesis until that actual errno is observed. The archived 1122 volume's 126/256 committed blocks cannot measure the failed open-file copy-on-write peak.
