# S31: TCP handshake failure isolated to unresolved peer ARP

NuttX HEAD remains 9d3b75798a3; Apps ae0dfd89c. No additional production
code/configuration commit in this diagnostic group. Wi-Fi/SMP instrumentation
remains in the six pre-existing dirty files. Preserve these changes separately.

## Image and experiment ledger

Build commands run from /home/regex/work/esp32s31-openvela, with full underlying
CMake commands/warnings in logs/buildNNN-*.log:

```sh
S31_DEMO_PROFILE=demo-rmt-tcpdiag bash backups/2026-09-10-scan-stress/build-demo.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build458-tcp-proc.sha256
# PASS, first real error: none. Enable existing NET_STATISTICS only.
bash backups/2026-09-10-scan-stress/flash-demo-pair.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build458-tcp-proc.sha256
# flash459 PASS, fixed kernel/AppFS offsets and backup/hash checks.
S31_EXPECT_PROTOCOL=7 S31_TCP_PROC_DIAG=1 S31_TEST_BSSID=60:ce:41:ab:02:d0 s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/network-repeat.py network460-tcp-proc tcp-diag
# Three rounds: FAIL/PASS/PASS for TCP; gateway checks pass.
python3 openvela-dev/nuttx/tools/test_esp32s31_tcp_synack_diag.py
# host461 PASS: actual diagnostic parser, independent known checksums,
# truncation/protocol/header/port filtering and odd-length sum.
S31_DEMO_PROFILE=demo-rmt-tcpdiag bash backups/2026-09-10-scan-stress/build-demo.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build462-tcp-synack.sha256
# PASS, first real error: none. Existing locked HAL warnings remain.
bash backups/2026-09-10-scan-stress/flash-demo-pair.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build462-tcp-synack.sha256
# flash463 PASS.
```

SYN-ACK counters inspect only TCP source port 5471: count frames, send return
status, IPv4 and TCP checksum failures. No payload retained, no packet-path
logging. Counters are atomic; reports are read via /dev/s31stat. A successful
send return means submitted to HAL, not independently proven on-air completion.

On image462, with S31_EXPECT_PROTOCOL=7, S31_TCP_PROC_DIAG=1, the same fixed AP,
and the same network-repeat.py command template:

- network464-tcp-synack: three TCP connect failures. TCP_SYN_RCVD, TCP stack
  generated/retransmitted responses, but matching SYN-ACK submit count zero.
- network465-arp-table: one TCP failure. Whole-table `arp -i wlan0` is unsupported
  without NETLINK_ROUTE and returned invalid argument; discard its ARP result.
- network466-arp-peer: single probe with `arp -i wlan0 -a 192.168.1.29` works.
  Peer MAC already present, TCP4096 PASS, SYN-ACK seen=queued=1, checksums good.
- network467-arp-peer: three TCP4096 PASS; peer MAC present by post-test read,
  one valid SYN-ACK submitted per round. Two started without a peer mapping.
- network468-arp-cold, additionally S31_TCP_ARP_MODE=cold: delete only board's
  RAM entry for 192.168.1.29 before TCP. All three fail in connect phase;
  peer entry remains unresolved, SYN-ACK submit count zero, TCP_SYN_RCVD.
- network470-arp-static, additionally S31_TCP_ARP_MODE=static and
  S31_TCP_PEER_MAC=a8:e2:91:97:1c:b4: install and verify that one temporary mapping.
  All three TCP4096 send/echo/close tests PASS, one SYN-ACK seen/queued each,
  nomem/error/badip/badtcp all zero. Every round resets and disables WLAN at end,
  removing temporary mappings. No persistent configuration or host route changed.

Single probes 465/466 use network-probe.py <absolute-redacted-log> 192.168.1.1
/dev/ttyUSB0 tcp-diag rather than the repeat wrapper. All credentials entered
through hidden getpass prompts, held only in memory, and redacted from logs.

## Interpretation and limits

This controlled comparison localizes this TCP failure class to unresolved peer
ARP before the SYN-ACK reaches the WLAN transmit hook. It is NOT proof of why
the ARP response is absent, nor complete Wi-Fi/HE acceptance. Successful TCP
with the same radio/AP/Windows/TUN path and a temporary mapping rules out a
blanket inability to exchange TCP data on that path.

Current NuttX arp_out replaces an IP packet with ARP when no mapping exists.
arp_find suppresses repeat attempts while resolution is in progress and then
temporarily reports unreachable. Relevant existing history:
5c4310f5a58 describes queuing IOBs to avoid lost initial ping/SYN-ACK;
8a0b0be4648 adds ARP flood suppression; 9547209b0c3 documents ARP-expiry TC13.
Do not blindly revert these changes or declare ARP queue/learning a proven fix.

Read-only Windows checks:

- `pktmon.exe status` and `pktmon.exe filter list`: access denied to PktMon
  driver; no capture started, no filters changed, no elevation bypass attempted.
- `Find-NetRoute -RemoteIPAddress 192.168.1.60`: WLAN interface 23, source
  192.168.1.29, on-link 192.168.1.0/24, next hop 0.0.0.0 (route461 log).
- `Get-NetAdapter -Name WLAN`: Up, interface23, A8-E2-91-97-1C-B4 (peer466 log).
  Route evidence does not exclude all packet-filter-driver effects.

## Next diagnostic and safe recovery

Build469: S31_DEMO_PROFILE=demo-rmt-arp-learn using the same build script and
build469-arp-learn.sha256 receipt, PASS with no build error. Exact configuration
diff from462 is only CONFIG_NET_ARP_IPIN=y. Source diagnostics unchanged.
Firmware469 and diagnostics469.patch are archived; flashing/testing follows
470. This enables existing same-subnet IPv4-to-MAC learning, not static peers.
It is diagnostic only: cold-cache, ARP expiry, malformed-frame/security scope,
and busy-network table pressure must be considered before production adoption.

At the end of 470 the board ran462 with WLAN disabled. Flash471 then starts
the next group. PD build directory already holds469; do not mistake it for462.
Stable Make432 remains unchanged/recoverable via flash-make434-pair.sh.
All flashes remain 0x2000/0x200000 only; no fuses, data partitions, reference
repositories, router/firewall/TUN settings or original documents modified.

Progress434-457 backup was successfully sealed after correcting an initial
empty-bundle-ref error; SHA256SUMS-progress457 and Git bundle verification PASS.
Full adaptation, Wi-Fi HE/stability and formal xTS remain incomplete. RGB
optical observation remains pending; don't pause unrelated work for it.
