# BLE 1062: match the pinned controller Host Buffer Size contract

1061 confirmed the 1055 entropy correction: `PSA PRNG result=0`. Initialization then failed on opcode 0x0c33 with status 17 (0x11), before adapter state 2. This is not a passing switch test.

The prior command parameters were ACL length 69, SCO length 0, ACL credits 12, SCO credits 0. Although `CONFIG_BT_BUF_ACL_RX_COUNT=0`, `BT_BUF_ACL_RX_COUNT` evaluates to `MAX(0, CONFIG_BT_MAX_CONN + 1) + CONFIG_BT_BUF_ACL_RX_COUNT_EXTRA`, i.e. `MAX(0, 2) + 10 = 12`. Zero configured legacy count is not zero actual credits.

The pinned, already-linked controller handler is `r_sym_bt_RlxCyY0wWN1G2GoSNTUz`. Its disassembly from the frozen 1055 ELF is preserved in `logs/ble1062-pinned-host-buffer-handler.txt`. The opcode dispatch explicitly matches 0x0c33 at 0x4003b210. With parameter length 7, it decodes ACL length and SCO length, then returns 17 if ACL length is <= 1020 (0x4003b2ea..0x4003b2f0) or SCO length is not 255 (0x4003b2f4..0x4003b2f8). Both former host values fail the actual controller contract. No HAL or IDF source/binary was modified.

1062 makes the host's real ACL RX buffers 1021 bytes through its board profile. Its S31-only Host Buffer Size setup sets the SCO length field to 255 while leaving SCO credits zero and enabling only ACL flow control. No SCO traffic/support is advertised through credits. The ACL pool remains 12 buffers. The framework H4 frame is 1026 bytes, exactly sufficient for type (1) + ACL header (4) + payload (1021); UART H4 RX ring remains 8192 bytes. A build assertion prevents a smaller ACL profile from silently advertising incompatible parameters. Initialization still propagates all controller errors normally.

Expected Host Buffer Size parameters: `fd 03 ff 0c 00 00 00` (ACL 1021, SCO 255, ACL credits 12, SCO credits 0). The existing unchanged original switch test remains the board acceptance criterion.

Candidate output: `openvela-dev/out/esp32s31-xts-flat-bttool1062`.
Receipt: `build1062-flat-bttool-buffers.sha256`.
Build log: `logs/build1062-flat-bttool-buffers.log`.
Build/flash helpers: `build1062-bttool-buffers.sh`, `flash1062-bttool-buffers.sh`.

Build exited zero; image size 1210564 bytes. 1062 and frozen 1055 receipts verified. Scoped source diff checks and helper shell syntax checks passed. Generated configuration preserved as `checkpoint1062-bttool-buffers/config`.

No UART access by this agent. Hardware validation remains root-owned. Build resource released. BUILD PASS / TARGET NOT RUN; no BLE PASS claimed.
