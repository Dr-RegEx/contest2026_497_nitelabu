# S31: dynamic ARP comparison, separate HE failure, and driver boundary

NuttX HEAD 9d3b75798a3; Apps ae0dfd89c. New work in this group is diagnostic
and not committed as a production Wi-Fi fix. Stable Make432 remains available.

## Dynamic ARP versus HE

- flash471: verified build469 (b/g/n, NET_ARP_IPIN=y) PASS.
- network472-arp-learn-cold: S31_EXPECT_PROTOCOL=7, S31_TCP_PROC_DIAG=1,
  S31_TCP_ARP_MODE=cold, fixed authorized AP 60:ce:41:ab:02:d0.
  Three rounds PASS: delete board peer mapping, recover dynamically from
  incoming same-subnet traffic, TCP4096 send/echo/close, valid SYN-ACK each.
  This is an existing feature/diagnostic workaround, not proof the underlying
  ARP request/response issue is fixed. ARP expiry/table/security review remains.
- build473: S31_DEMO_PROFILE=demo-rmt-arp-learn-he with build-demo.sh and
  build473-arp-learn-he.sha256. PASS; first real build error: none.
  Exact .config diff from469 is only ESPRESSIF_WIFI_STA_11AX=y.
- flash474: flash-arp-he-pair.sh build473-arp-learn-he.sha256 PASS; same two
  authorized offsets, explicit HE/ARP flags and backup/hash checks.
- network475-he-arp-learn-cold: same three-round cold-ARP probe but expected
  protocol71. All associate and negotiate PHY6 (HE20); cold/warm gateway
  checks and TCP connect fail in all rounds. RX type counters only legacy,
  HT/SU/ER-SU/MU remain zero. Negotiation alone is NOT HE traffic acceptance.
  A second problem remains in the HE path; do not attribute all failures to ARP.

## ARP driver boundary

Added temporary ARP header classification and queue/submit return counters;
no packet payload retained or printed in callbacks. TX direction0/RX1.
Peer classification is explicitly the authorized test PC (192.168.1.29),
not a production address. `bad` checks header length and hardware/protocol
fields; it is not exhaustive validation of every ARP/Ethernet field.

The new checksum helper was renamed wlan_tcp_diag_sum to avoid a collision
with the older, disabled WLAN_PACKET_DIAG checksum helper. Old diagnostic
source snapshots remain preserved for the preceding firmware images.

- host477: test_esp32s31_arp_diag.py and test_esp32s31_tcp_synack_diag.py PASS.
- build478: S31_DEMO_PROFILE=demo-rmt-tcpdiag, build478-arp-trace.sha256,
  build-demo.sh PASS; first real build error: none. b/g/n, learning OFF.
- flash479: flash-demo-pair.sh build478-arp-trace.sha256 PASS.
- network480-arp-boundary-cold: same protocol7/cold/fixed-AP repeat template.
  Three TCP connect failures. A 42-byte peer-targeted ARP request is submitted
  to HAL per round; selected header checks pass, Ethernet source matches
  ARP sender MAC, submission succeeds. No matching SYN-ACK reaches TX hook.
  Rounds1/2 show no new received ARP frames across the TCP attempt. In round3,
  a peer reply and valid peer mapping are present by the post-timeout read,
  but TCP remains SYN_RCVD. These snapshots do not timestamp the exact reply
  arrival; do not assert that it arrived only after the client deadline.

Next experiment: S31-only zero-padding of short Ethernet frames to ETH_ZLEN
(60 bytes without FCS), still learning OFF and same protocol/AP/timeouts.
host481 verifies original bytes preserved, zero-only padding, lengths
14/42/44/54/59/60/61/1514, unchanged bytes beyond the output, and the ARP/TCP
diagnostic regressions. Build482 PASS, first real error none. At this note's
checkpoint it is a candidate, not an accepted fix; flash483/test484 follow.

Complete commands and first errors are preserved in build/flash logs. Probe
commands use network-repeat.py <label> tcp-diag with the listed environment
variables; credentials are hidden in memory and logs are redacted. Each probe
ends with board reset/ifdown; it does not leave HTTP demo running. All flashing
remains 0x2000 and 0x200000 only. No reference, router, TUN or fuse changes.

Supplementary dependency audit: dependency476-nested-hal-audit.md. Original
lock check PASS, but nested prefix changes are separately archived rather
than incorrectly described as a pristine reference submodule.
