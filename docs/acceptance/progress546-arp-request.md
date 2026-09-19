# ARP request invariants543–550

NuttX HEAD39be74d0093, Apps ae0dfd89c. Prior turn made progress: RGB optical
acceptance recorded, bounded SNAP/ARP-prefix callback evidence obtained,
Demo restored and checkpoint542 sealed. This group adds temporary mismatch
counters to the existing dirty WLAN/monstat files, no production fix/commit.

wlan_arp_request_diag compares requests targeting the authorized PC against
the live netdev: broadcast Ethernet DA, Ethernet SA, ARP SHA, ARP SPA,
zero target hardware address. Only mismatch counts retained. Existing ARP
parser separately checks EtherType, hardware/protocol kinds and lengths;
the new helper by itself is not complete ARP validation.
IP comparison uses network-order d_ipaddr bytes; packet not modified.
monstat formatter already clamps final output length, so the extra counters
do not cause out-of-buffer reads (late diagnostic lines may be truncated).

host543 test_esp32s31_arp_diag.py native ASan/UBSan/LSan PASS for each field
mutation, valid request, null/short input and non-target/reply filtering.
Scoped nxstyle and git diff --check PASS. No test error this group.
build544: S31_DEMO_PROFILE=demo-rmt-tcpdiag bash D/build-demo.sh
D/build544-arp-request.sha256, exit0, first real build error:none.
D is the absolute parent of this note; build log preserves full commands.
host545 read-only Windows IPv4 check confirms192.168.1.29,WLAN,interface23.
flash545 exit0 with receipt544, same backup/hash guards and two offsets.

network546 one cold-ARP trial: S31_EXPECT_PROTOCOL=7,
S31_TCP_PROC_DIAG=1,S31_TCP_ARP_MODE=cold,
S31_TEST_BSSID=60:ce:41:ab:02:d0, network-probe.py
D/logs/network546-arp-request-cold-redacted.log 192.168.1.1 /dev/ttyUSB0 tcp-diag.
Hidden credentials; exit1. DHCP/gateway PASS,TCP connect timeout.
Post-TCP ARPREQ seen1 and all five mismatch counters0.
SNAP14->15,ARP prefix2->3,ARP failure0->0,last offset34/length56.
No observed corresponding ARP response. This checks outgoing NuttX-side
fields and driver-reported completion, not actual AP/PC reception or full
on-air frame integrity. Do not attribute cause to TUN/AP/PC without evidence.

build547 profile demo-rmt, receipt build547-restore-demo.sha256, exit0,
first real build error:none; flash548 restoration exit0. Demo549 exit0,PID12
at192.168.1.60:8080; HTTP550 exit0,10 requests and full boundaries PASS.
No active build/flash/serial/HTTP test session from this group. Board stays
connected with Demo running. firmware544 and diagnostics544 archived before
PD rebuild. Reference/network settings untouched; Windows UAC capture
request remains unanswered and unexecuted.

Next useful control: current committed code without six temporary diagnostic
edits, keeping relevant TCP/ARP configuration equal. Do not reset main WIP.
Create a detached worktree from existing Git objects if needed; no clone or
download. Existing Make432 has no diagnostics but predates TCP accept fix,
so a fresh39be baseline is preferable. Main CMake supports explicit
-DNUTTX_APPS_DIR to reuse current Apps. Inspect local littlefs/HAL dependency
paths/gitlinks before setup; do not silently initialize/fetch missing repos.
Preserve main source configuration and all existing Make worktrees.
No new clean baseline worktree has been created yet.

Read-only dependency discovery: existing main fs/littlefs/littlefs is a
separate Git repository at9d31e6df950d7140ed17502022be91186164c160, not a
gitlink in NuttX HEAD. fs/littlefs/CMakeLists.txt skips fetch if that directory
exists; otherwise declares an external archive and source patches. Reuse
the existing component in the isolated setup; do not allow that fetch or
patch to modify the existing component. Inspect its dirty status first.
Main Git worktree list reports repo-managed .repo/projects/nuttx.git as
the primary entry plus the two existing Make worktrees; preserve all three.
