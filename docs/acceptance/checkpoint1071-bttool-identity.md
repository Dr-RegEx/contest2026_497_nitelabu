# BLE 1071: restore the chip's real public identity and deletable tasks

1068/1070 showed that candidate 1062 completed host initialization and reported adapter state 2, but immediately asserted in `sal_adapter_le_interface.c:1807` because identity type was not public. This is a failed original switch test, not a PASS. 1070 retains the complete panic in `logs/xts1070-ble-panic-capture.log` and a second cleanup assertion in OSAL task deletion.

Root cause of identity failure: the board's SDK compatibility header still selected the obsolete ONE universal MAC allocation and only the Ethernet universal address. It omitted `CONFIG_ESP_MAC_ADDR_UNIVERSE_BT`. In the pinned `components/esp_hw_support/mac_addr.c`, that macro controls compilation of the `ESP_MAC_BT` switch case; without it, `esp_read_mac(..., ESP_MAC_BT)` returns `ESP_ERR_NOT_SUPPORTED`. The controller's pinned BLE initialization calls the OSAL eFuse hook but ignores its return before setting the public address. Zblue subsequently reads an absent public address, creates a random identity (the log shows opcode 0x2005 immediately after 0x1009), and the framework correctly rejects its type.

1071 updates only the board compatibility header to the pinned S31 `components/esp_hw_support/port/esp32s31/Kconfig.mac` default: TWO universal addresses, Wi-Fi station and Bluetooth. The factory eFuse base address supplies Wi-Fi, base+1 supplies Bluetooth; no MAC is invented or transformed into a fake public identity. The HCI driver's open path now checks `esp_read_mac` before controller initialization and logs the real Bluetooth MAC or propagates an error, preventing silent invalid initialization. The framework public-type assertion is unchanged. Ethernet now follows the documented locally administered derivation for a two-address S31.

Second confirmed mismatch: OSAL created controller tasks with `kthread_create`, but their deletion occurs from the application HCI close path. NuttX `nxtask_delete` expressly returns `-EACCES` when an application attempts to delete a kernel thread. In this FLAT-only driver the controller tasks now use `task_create`, retaining actual scheduling, stack, entry, and PID handle semantics. The private timer worker remains a kernel worker. The deletion assertion remains; no error is swallowed.

Candidate: `openvela-dev/out/esp32s31-xts-flat-bttool1071`.
Receipt: `build1071-flat-bttool-identity.sha256`.
Helpers: `build1071-bttool-identity.sh`, `flash1071-bttool-identity.sh`.
Build log: `logs/build1071-flat-bttool-identity.log`.

Build exited zero, image size 1210836 bytes. 1071 and frozen 1062 SHA receipts verify; scoped source diff and helper syntax checks passed. Configuration and SDK compatibility header are preserved in `checkpoint1071-bttool-identity/`. Build resource released.

Pinned HAL/IDF and original test acceptance criteria are unchanged. Root retains UART ownership; no serial operation was performed by this agent. BUILD PASS / TARGET NOT RUN. Hardware validation is required before claiming a switch PASS.
