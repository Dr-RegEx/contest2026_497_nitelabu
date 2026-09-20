# BLE 1031: bounded first-enable diagnostics

BUILD PASS, TARGET NOT RUN. No claimed BLE fix or xTS PASS yet.

939 failed the original enable transition in `logs/xts1025-ble-switch.log`: state 1 and the host's `Sending HCI_RESET` message, then no state 2 within 60 seconds. The lack of controller logs does not prove its open was never called: the 939 ELF confirms `bt_enable_mc` invokes the actual H4 open API before scheduling host initialization, and H4/IDF log filters suppress their normal messages. H4 packet framing matches the pinned VHCI transport's leading packet type contract. No framing rewrite or forced enable success was introduced.

1031 adds unconditional, narrowly scoped stage messages to the S31 driver (controller open/ready, HCI Reset TX/result and Reset-complete RX), plus S31-only Reset allocation/queue identity/wait-result messages in Zblue `hci_core.c`. These distinguish command allocation, syswork queue identity, actual controller TX and the return path. Original `bttool` commands, acceptance state, and timeout are unchanged. UART ownership stays with root.

Independent output: `openvela-dev/out/esp32s31-xts-flat-bttool1031`.
Image size: 1210368 bytes.
Receipt: `build1031-flat-bttool-diagnostic.sha256`.
Build entry: `S31_FLAT_PROFILE=xts-flat-bttool bash build1031-bttool-diagnostic.sh <fresh absolute receipt>`; local copy of existing helper changes only output directory.
Build log: `logs/build1031-flat-bttool-diagnostic.log`.
939 output and receipt verified unchanged. Root may repeat the original switch executor with the 1031 receipt and should retain all stage lines and `ps` after any timeout.
