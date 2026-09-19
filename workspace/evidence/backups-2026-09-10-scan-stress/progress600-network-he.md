# Same-AP full network and HE control, 2026-09-14

Previous goal turn made progress: clean551 cold ARP/TCP passed3/3 after user
switched Windows to board's same2.4GHz AP; latest demo restored and HTTP passed.
This group rechecked HEAD f10a8f4b209, existing six diagnostic edits unchanged.
Apps remains ae0dfd89c. No new source fix/commit, dependency downloads, router,
firewall, TUN, reference repository or security-fuse changes.

D = this directory, R = /home/regex/work/esp32s31-openvela/s31-reference.
Commands below use existing scripts; complete configure/build/flash output
is retained in logs. Network credentials use hidden getpass, redacted logs.

## Basic b/g/n network

S31_EXPECT_PROTOCOL=7 S31_TEST_BSSID=60:ce:41:ab:02:d0
R/.venv-nuttx/bin/python -u D/network-repeat.py
network600-same-ap-full tcp-udp-dns

Runs existing board firmware591, no flash before this test. Exit0,
BATCH_RESULTS=[0,0,0]. Each independent reset/reconnect round: active/passive
scan, DHCP192.168.1.60, gateway, DNS example.com using DHCP-provided resolver,
TCP4096 exact content/hash/server close and UDP256x96 bytes all PASS.
UDP total768 packets/73728 payload bytes; pacing40ms, not throughput testing.
Board example validates each payload and sequence; host transcript checker
requires exact0..255/full96bytes/singlepeer/no errors and process exit.
No static ARP map, ARP-learning option or timeout extension.
Host601 command python3 D/test-udp-probe.py exit0: actual C fill/check functions
match all256 generated payloads; missing/duplicate/size/peer/order/content
negative cases rejected. This does not cover UDP board-to-PC transmit or IPv6.

## HE-only diagnostic

S31_DEMO_PROFILE=demo-rmt-he bash D/build-demo.sh
D/build601-same-ap-he.sha256 > D/logs/build601-same-ap-he.log 2>&1
Exit0, first real build error:none. Config1680booleans/protocol71 verified.
Existing locked HAL warnings remain. Profile includes demo-rmt and enables
only STA_11AX; no NET_STATISTICS/ARP_IPIN/ARP_SEND_QUEUE. Archive
firmware601-same-ap-he.tar.gz preserves ELF/bin/AppFS/config before rebuild.

bash D/flash-he601-pair.sh > D/logs/flash602-same-ap-he.log 2>&1
Exit0, actual ELF71 and hashes/backup/size guards passed. Only existing
0x2000kernel and0x200000AppFS ranges written, data>=0x500000 untouched.
This script deliberately rejects ARP diagnostic options and requires prior
checkpoint594 backup; it does not relax or reuse the ARP-IPIN HE guard.

S31_EXPECT_PROTOCOL=71 S31_TEST_BSSID=60:ce:41:ab:02:d0
R/.venv-nuttx/bin/python -u D/network-repeat.py
network603-same-ap-he tcp-diag

Exit1, BATCH_RESULTS=[1,1,1]. All three: actualbitmap71/PHY6HE20 and DHCP PASS,
both gateway ping groups0/4 (total0/24), TCP connect timeout, RX legacy only,
allHE types0. First failed network check: cold gateway ping; final exception
TCP connect/send/echo/close failure. Normal ifdown/reset cleanup all completed.
No claim that negotiatedHE20 alone is HE data acceptance. B/g/n same-AP
success does not solve the HE path; previous IDF171-173 control already showed
same board and HAL PHY/Wi-Fi libraries can pass HE under IDF (otherAP/channel
conditions, see diagnostics/idf-baseline/README.md). No new IDF flash here.

Restore build command:
S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh D/build604-restore-bgn.sha256
> D/logs/build604-restore-bgn.log 2>&1
Exit0, first real error:none; config/protocol7 checks PASS. Runs only after
HE flash602 completed, while603 uses board601; no overlapping flash/build
consumption of the same mutable images. Source Make config restored by script.
Actual generated601/604 config diff is only STA_11AX, recorded in
logs/host607-he-bgn-config.diff. Existing diagnostic diff SHA256 still equals
checkpoint594/nuttx-diagnostics.patch:
0a6ec1051f7c59dfa9eb4abcb20ed3fc31cd12397226d8e620358eb861702b0c.

Restore command bash D/flash-demo-pair.sh D/build604-restore-bgn.sha256
> D/logs/flash605-restore-bgn.log 2>&1, exit0 after603 batch terminal exit1.
Then R/.venv-nuttx/bin/python -u
/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif/esp32s31_demo.py
--port /dev/ttyUSB0 --log D/logs/demo606-restored.log
exit0, PID12, http://192.168.1.60:8080/ remains connected. Standard demo
association is not BSSID-pinned, unlike network600/603 controls.
Windows PowerShell with existing cached Python invokes
openvela-dev/apps/examples/s31demo/test_http.py 192.168.1.60
--samples 10 --boundaries, logs/http607-restored.log, exit0.
HTTP10 and method/path/browser-headers/header-limit/idle-recovery PASS.

checkpoint607.sh seals601/604 firmware, full logs, scripts, current Git status
and WIP patch with SHA256SUMS-progress607 plus prior599 checksum link.
No build/flash/serial/HTTP process remains. PD current604, older PD receipts
are not valid for its regenerated files; use604 or archived matching firmware.
Next continue HE-specific OS/initialization/interrupt timing differential
analysis, not arbitrary network changes or counting this as fullWiFi success.
