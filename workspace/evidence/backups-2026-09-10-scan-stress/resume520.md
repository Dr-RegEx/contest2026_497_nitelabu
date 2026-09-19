# Resume520: next network evidence boundary

NuttX HEAD cc3da0ce101; parent1fee91b2d2a; Apps ae0dfd89c.
Two ARP safety fixes committed, not a Wi-Fi connectivity fix. Six pre-existing
Wi-Fi/monitor diagnostic files remain dirty; demo-rmt-arp-queue is a new
untracked diagnostic profile. No new generic ARP WIP.
No active merge/rebase/cherry-pick/revert or unmerged entries on recheck.

build509 queue candidate not flashed (then found expiry race).
build513 queue safety candidate flash515 exit0; network516 cold3 rounds
all DHCP/gateway PASS and TCP connect FAIL. No corresponding new peer reply
or SYN-ACK TX; queueing does not resolve missing ARP reply. config478-to513
diff exactly queue=y; source also has recorded intervening RMT/safety fixes.
build517 restore demo exit0, flash518 exit0, demo519 exit0 PID12,
http://192.168.1.60:8080/. Queue/IPIN/HE OFF. HTTP520 finished exit0,
100 requests and method/path/header-limit/idle recovery PASS. No active
build/flash/serial/HTTP test session from this group. Host520 suite exit0,
F0 PASS subject to the known nested-mbedTLS audit476 caveat.

Next useful diagnostic without Windows admin: record ARP TX-done status
separately from HAL submit return. Read locked HAL
components/esp_wifi/include/esp_private/wifi.h lines470-493 and950-969:
esp_wifi_internal_tx copies input before forwarding; return0 means accepted
by driver. wifi_tx_done_cb_t has ifidx,data,uint16_t *len,bool status and
describes status as transmission success/failure. Broadcast success still
does not prove PC receipt or an over-air ACK.

Current path: common/espressif/esp_wlan.c esp_wifi_tx_done_cb dispatches to
g_sta_txdone_cb -> wlan_sta_tx_done, which ignores data/len/status unless old
disabled WLAN_PACKET_DIAG printing is enabled. Do NOT enable synchronous
packet printing. Add bounded header-only atomic counters, no payload/log
in callback, behind S31+NET_STATISTICS. Existing g_s31_arp_diag[2][10]
distinguishes TX submission(dir0) and RX(dir1); can extend with dir2 TX-done
only after updating parser bounds, monstat extern/loop and host tests.
For TX-done peer classification use ARP target IP like TX, not RX sender IP.
Guard null len/data, validate lengths/Ethertype/header. Consider a total
callback counter to distinguish no callback from non-ARP data layout.
This plan is not implemented yet; build517 and source still use2 rows.

Use learning-OFF, queue-OFF b/g/n tcpdiag profile for next comparison; do not
silently leave queue experiment on. Archive current517 before next PD build
(checkpoint520 script does this). Candidate513 already archived with diff.
Do not flash before current HTTP is done and next build exits0. After new
network test restore demo. Pending optional Windows UAC capture request is
still unanswered; no admin launch/capture/filter/TUN/router modifications.

RMT install rollback/cancellation/refill debts remain in resume506 and
rmt-followup494, lower priority than this next network discrimination.
Full SMP/MMU/driver/xTS/HE acceptance remains incomplete; goal stays active.
