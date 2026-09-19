# Same-AP 2.4 GHz control, 2026-09-14

User switched Windows Wi-Fi to 2.4 GHz. Read-only Windows check confirmed
WLAN, channel 11, BSSID 60:ce:41:ab:02:d0, IPv4 192.168.1.29/interface 23,
MAC a8:e2:91:97:1c:b4. This is the BSSID already pinned by the board probe.
Evidence: logs/host596-same-ap.log. Previous PC band/BSSID was not recorded
immediately before the switch, so this is not proof of a 5 GHz-specific fault.

NuttX HEAD f10a8f4b209, branch codex/esp32s31-port; Apps ae0dfd89c.
Six existing tracked diagnostic edits and untracked profiles/tests preserved.
No merge/rebase/cherry-pick/revert state found; git diff --check passed.
No source changes, downloads, builds, router/TUN/firewall/host ARP changes.

Commands (D = this directory, R = /home/regex/work/esp32s31-openvela/s31-reference):

1. bash D/flash-clean551.sh > D/logs/flash595-clean-same-ap.log 2>&1
   Exit 0; existing clean551 images and backup hashes verified. Writes only
   0x2000 and 0x200000, preserving data at/above 0x500000.
2. S31_TCP_PROC_DIAG=1 S31_TCP_ARP_MODE=cold
   S31_TEST_BSSID=60:ce:41:ab:02:d0 R/.venv-nuttx/bin/python -u
   D/network-repeat.py network596-clean-same-ap-cold tcp-diag
   Credentials entered privately via hidden PTY prompts, logs redacted.
   Exit 0, BATCH_RESULTS=[0, 0, 0]. Each round resets/reconnects board,
   deletes peer ARP, and confirms missing mapping before TCP.
   All three: DHCP/gateway PASS, TCP4096 exact-content/hash PASS, server exit
   PASS, automatic peer mapping a8:e2:91:97:1c:b4 present after TCP.
   No static mapping or client timeout extension. First real error: none.

Interpretation: the previous cold-peer-ARP failure did not reproduce in three
rounds on the same 2.4 GHz AP. Earlier clean553 cold control failed 3/3;
clean589 active-ARP control passed only 1/3. This is encouraging evidence,
not full Wi-Fi certification or proof against the driver. No packet capture
locates prior loss, and no basis to blame or exonerate TUN conclusively.
HE remains a separate unresolved DHCP/gateway issue; clean551 is b/g/n.

3. bash D/flash-demo-pair.sh D/build591-arp-callback.sha256
   > D/logs/flash597-restore-demo.log 2>&1
   Exit 0, verified latest build591 restored after network-repeat exited.
4. R/.venv-nuttx/bin/python -u
   /home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif/esp32s31_demo.py
   --port /dev/ttyUSB0 --log D/logs/demo598-same-ap.log
   Exit 0, PID12, protocol bitmap7; http://192.168.1.60:8080/ stays running.
   Standard demo association is not BSSID-pinned, unlike the cold controls.
5. Windows PowerShell invokes existing Windows Python and
   openvela-dev/apps/examples/s31demo/test_http.py 192.168.1.60
   --samples 10 --boundaries. Output D/logs/http599-same-ap.log.
   Exit 0, HTTP samples10 and method/path/browser-headers/header-limit/
   idle-recovery boundaries all PASS. First real error: none.

checkpoint599.sh archives these logs, probe/flash scripts and note, checks
prior594 archive and current591 image hashes, writes SHA256SUMS-progress599.
No new code commit this group. No build/flash/serial/client process remains;
board retains build591 and connected demo598. Next: repeatability/longer
network validation in this known same-AP environment; HE and other drivers
remain separate work. Do not treat this result as authorization to alter
Windows TUN/firewall/router settings or perform elevated packet capture.
