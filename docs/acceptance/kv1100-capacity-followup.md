# KVDB1100: preserved failure and next targeted retest

Original ten rounds returned TEST PASSED and exit0, but 90 dirty-page commit errors invalidate acceptance. The original test ignores property_commit() return. checkpoint1100-kv-stability-errors freezes the raw transcript and old runner. The live runner now rejects IO/rollback diagnostics after gathering final state.

Full post-error 1MiB volume: flash1110-kv-errors/volume.bin, SHA256 85e8c785368e14e91856c7a920c75382aa87a825153a79ca718df5b8aaa87d0a. Original pre-test volume remains flash1095-kv-evidence.

Read-only host inspection1113 uses the existing LittleFS library and archived bytes, with program/erase callbacks returning errors and source files opened rb. No UART or changes to volumes. Current fork does not link with LFS_READONLY (lfs_fs_parent missing); normal library build with denied write callbacks is used instead. This is inspection, not a filesystem acceptance test.

Persisted after-reset view:197/256 blocks; persist.db147456B; old s11/performance_test262144B and s12/payload262144B. Before stability, mounted free space was96 blocks384KiB. Live final df showed261 used/256 and unsigned-underflow available; do not treat it as usable free space or assert on-disk corruption. Reopened archived view differs from live dirty/open state.

Next bounded action after current Wi-Fi1112 finishes: flash948 and no-format mount; verify archived volume hash, no active daemon/workload, exact old payload sizes; remove only those two archived performance files and persist.db (published stability precondition), then original required reboot/remount. Execute original stability10 without changes. This checks capacity as a cause; it is not yet proven. Keep both original failure and new result. No formatting, unrelated deletion, or benchmark count change.
