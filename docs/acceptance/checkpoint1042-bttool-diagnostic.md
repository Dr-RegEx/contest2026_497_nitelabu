# BLE 1042: locate the stage after successful Reset

BUILD PASS, TARGET NOT RUN. No confirmed BLE fix or xTS PASS.

Board run 1040 of candidate 1031 (`logs/xts1040-ble-diagnostic.log`) reached controller open/ready, Reset allocation, matching current/syswork thread pointers, Reset TX, seven-byte command complete, TX return 3, and synchronous wait return 0. The original enable test still timed out in state 1 without reaching state 2. This establishes successful initial Reset delivery/completion; it does not establish completion of host initialization.

Run 1041 (`logs/xts1041-ble-ps.log`) observed ESP-ROM while attempting existing-console recovery and stopped before collecting `ps`. The reset cause is unknown; this is neither watchdog evidence nor a passing original test.

1042 extends the existing S31-only diagnostics to every synchronous HCI command: opcode, allocation, queue/thread identity, command-complete status, wait result/status/reference count, and return after buffer handling. A separate marker follows `hci_reset_complete`. These distinguish Reset cleanup from a later command or later initialization stall. No command, timeout, acceptance state, HCI framing, or controller behavior changed. Pinned HAL/IDF remain unchanged.

Independent output: `openvela-dev/out/esp32s31-xts-flat-bttool1042`.
Image size: 1210372 bytes.
Receipt: `build1042-flat-bttool-diagnostic.sha256`.
Build helper: `build1042-bttool-diagnostic.sh`.
Build log: `logs/build1042-flat-bttool-diagnostic.log`.
Dedicated flash helper: `flash1042-bttool-diagnostic.sh` (syntax checked, not executed by this agent).

Build exited 0. Receipt checks for 1042, 1031, and 939 passed; older candidates remain intact. Scoped source `git diff --check` passed in NuttX and Zblue. Root retains sole UART ownership and should run the unchanged switch executor with the 1042 receipt.
