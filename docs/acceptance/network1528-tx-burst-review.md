# Bounded TX poll candidate

Baseline 1527 (frozen 1493 pair): one 10-second TCP sender diagnostic transferred 285 KiB, 204 Kbit/s on target and 206 Kbit/s on PC, with 2 TCP retransmissions in 206 transmitted TCP packets. Close drained normally. This is not an original xTS run.

In the IOB path, devif_poll returns the callback stop status after traversing active connections. wlan_txpoll returns zero, so the old while(devif_poll(...)) typically does not replenish the same TCP connection within that worker invocation. It relies on later TX completion/ACK/work notifications.

Candidate 1528 records whether a poll produced a packet and performs up to eight passes, stopping as soon as no packet is produced or the pending driver TX queue is nonempty. It retains the existing send backpressure handling, does not change the stack window, and bounds network-lock holding time. This is S31-only. The effect on throughput is a hypothesis until target comparison.

Build uses the same RX stats, BA=12 and static RX=16 as baseline 1493. The normal competition defconfig was restored after the successful build; candidate configuration and both kernel/AppFS hashes are frozen separately. No SIMD or BLE/Wi-Fi coexistence is enabled.
