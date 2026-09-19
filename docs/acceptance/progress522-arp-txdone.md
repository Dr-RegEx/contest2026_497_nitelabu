# ARP transmit completion boundary

NuttX HEAD cc3da0ce101, Apps ae0dfd89c. Previous turn was progress:
two ARP safety fixes committed, queue comparison failed, Demo restored and
HTTP520 passed; sealed checkpoint520 verified. This group is temporary
diagnosis in existing dirty esp_wlan.c/esp32s31_monstat.c, not a Wi-Fi fix.

Existing header-only ARP counters extended from2 to3 rows:0 submit,1 RX,
2 TX done. Both TX rows classify peer by ARP target IP; RX uses sender IP.
New TXDONE totals: all callbacks, failed status, invalid pointer/length,
non-ARP. No payload retained/printed, no packet-path allocation or blocking.
TX status is the HAL callback's report; broadcast success does not prove
over-air ACK or PC receipt. Locked HAL wifi.h documents input-copy semantics
for esp_wifi_internal_tx and the TX-done callback ABI; see resume520.

host521 test_esp32s31_arp_diag.py + test_esp32s31_tcp_synack_diag.py PASS,
including TX-done success/failure, peer direction, null pointers, lengths,
and non-ARP. Initial broad nxstyle range hit existing inline peer initializer;
full monstat check hit pre-existing aligned attribute at91. New-line scoped
nxstyle and git diff --check PASS. Those older style warnings were not fixed
or claimed absent. No functional host test failure in this group.

Build command (D = absolute directory containing this note):
S31_DEMO_PROFILE=demo-rmt-tcpdiag bash D/build-demo.sh
D/build522-arp-txdone.sha256
Exit0; first real build error:none. Full commands in build522 log.
Verified actual config: NET_STATISTICS=y; queue,IPIN,11AX OFF.
firmware522-arp-txdone.tar.gz and diagnostics522-arp-txdone.patch saved before
any further PD rebuild. flash523 uses flash-demo-pair.sh with522 receipt,
only0x2000/0x200000, backup/hash guards. flash523 exited0.

Test524: network-repeat.py network524-arp-txdone-cold
tcp-diag, with protocol7, TCP proc diagnostics, cold peer ARP deletion,
fixed authorized AP60:ce:41:ab:02:d0. No static mapping, IP harvesting,
timeouts, router, TUN or administrator-capture changes. Credentials hidden.
Actual exit1,rounds[1,1,1]: DHCP/gateway PASS, TCP connect TimeoutError all3.
All observed callbacks classified nonarp, invalid=0, ARP completion row2
remains zero even while ARP submission row0 grows. This DOES NOT prove ARP
callbacks are absent: the Ethernet-header assumption is not validated.
Pre/post TCP TXDONE totals: round1 15->16 fail1->1; round2 14->15 fail0->0;
round3 15->19 fail0->0. Do not attribute total status to a specific frame.

Offline inspection: host525-libpp-disassembly.log, locked libpp.a
.wifiextrairam.17 ppProcTxDone callback sequence e0..108 loads data from
an internal buffer, conditionally advances8 bytes, len pointer=s0+22,
status tests byte19 of another descriptor. These are reference-object
instructions: actual linked ppProcTxDone is ROM absolute2f800fe4, so do not
claim that this object body is running on the board. First attempted
section .text.ppProcTxDone did not exist; full disassembly found real section.
Installed ROM ELF search found C6 only, not S31. No downloads or ROM changes.

Next: identify callback frame layout with bounded header-kind counters,
not raw packet logging. Consider802.11 frame-control and LLC/SNAP recognition
with strict bounds; validate runtime layout before inferring ARP TX status.
Existing row2 classification remains provisional and is not a driver fix.

build525: S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh
D/build525-restore-demo.sha256, exit0, first real build error:none.
Queue/IPIN/NET_STATISTICS/HE off for restoration; new counters compiled out.
flash526 exited0, Demo527 exited0, PID12 at192.168.1.60:8080. HTTP528 exited0:
10 samples plus method/path/browser-header/header-limit/idle-recovery PASS.
Board remains connected with Demo running; no live tool session from group.

Evidence precision: zero classified ARP callbacks does not by itself prove
different data layout. Alternative: ARP callbacks absent/filtered. Aggregate
callback deltas correlate with submissions but do not identify individual
frames. Next test must distinguish these possibilities, not treat the
Ethernet-layout hypothesis as conclusively disproven. No production commit
this group; existing6 diagnostic files and untracked tests are archived.
Pending optional Windows UAC capture request still unanswered; not executed.
