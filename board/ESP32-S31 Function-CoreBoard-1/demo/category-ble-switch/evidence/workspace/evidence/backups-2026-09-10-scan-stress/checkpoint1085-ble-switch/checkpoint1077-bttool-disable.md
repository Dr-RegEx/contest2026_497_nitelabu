# BLE 1077: unregister callbacks before destroying their host context

1076 on candidate 1071 confirmed real public MAC, enable callback state 2 and queried state 2. Disable completed HCI Reset, then faulted at EPC 0x400c1c2e with MTVAL 0x10c. The complete original switch test remains FAIL.

The frozen 1071 ELF resolves EPC to `sys_slist_find_and_remove`; caller return addresses 0x400c2cc0/0x400c2ca6 resolve to `bt_conn_cb_unregister_mc`, and 0x400a8836 to `zblue_le_disable`. This matches source exactly: the LE service called `bt_disable()` first, then `zblue_unregister_callback()`. Successful host disable calls `bt_dev_free()`, which zeroes the device and its `conn_ctx` pointer. `bt_dev_get()` still returns that static device slot, so the subsequent unregister dereferences `hdev->conn_ctx->conn_cbs` through NULL. This is cleanup ordering, not a controller Reset failure.

1077 moves real callback unregistration before `bt_disable()`, while the host context exists. This agrees with the framework's existing BR disable ordering. If disable fails but the host remains ready, callbacks are restored. The error is still logged and OFF is reported only after successful disable. No NULL fault masking, fake successful command, omitted controller shutdown, or acceptance-criteria change was introduced.

Only behavioral source change: `frameworks/connectivity/bluetooth/service/stacks/zephyr/sal_adapter_le_interface.c`, function `STACK_CALL(le_disable)`.

Independent output: `openvela-dev/out/esp32s31-xts-flat-bttool1077`.
Receipt: `build1077-flat-bttool-disable.sha256`.
Helpers: `build1077-bttool-disable.sh`, `flash1077-bttool-disable.sh`.
Build log: `logs/build1077-flat-bttool-disable.log`.

Build exited zero; 1077 and frozen 1071 receipts verified. Scoped source diff check and helper syntax checks passed. Config and changed framework source are preserved in `checkpoint1077-bttool-disable/`. Build resource released.

BUILD PASS / TARGET NOT RUN. No UART access by this agent. Original switch test must be rerun on hardware before recording PASS.
