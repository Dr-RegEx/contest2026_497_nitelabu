# Committed-source network control551 onward

Main NuttX remains codex/esp32s31-port at39be74d0093, with its six existing
diagnostic source edits preserved. Apps remains ae0dfd89c. No new source
commit in this group. No active merge/rebase/cherry-pick/revert or unmerged
files were found in the initial read-only checks.

Created detached worktree openvela-dev/out/esp32s31-clean551/nuttx from
existing Git objects at39be74d0093. The first add command had a mistyped
full hash and failed before creating a worktree; retry used the verified
abbreviation. No clone or fetch. The sibling apps symlink reuses main Apps;
fs/littlefs/littlefs symlink reuses the existing clean local component at
9d31e6df950d7140ed17502022be91186164c160. Locked HAL/IDF reused unchanged.
Previously recorded nested HAL mbedTLS dirtiness caveat still applies.

Only added an untracked demo-rmt-tcpdiag profile in the detached worktree,
enabling NET_STATISTICS on committed demo-rmt. All tracked source files
match the commit. Main top-level CMake currently overwrites an explicit
NUTTX_APPS_DIR with CONFIG_APPS_DIR, so used its sibling Apps discovery
instead of assuming that command-line override works.

Build command: bash D/build-clean551.sh, where D is this note's directory.
First invocation exited at the script's mistyped short-hash guard, before
CMake. Corrected the guard to the verified11-character hash; original empty
log retained. Retry log build551-clean-retry.log contains full build commands;
exit0, no compiler/build error. Config/ELF protocol guard PASS bitmap7.
config544-to551.diff is empty: generated .config exactly matches prior544.
Build is isolated; the main PD build547 images/receipt remain unchanged.

Flash command: bash D/flash-clean551.sh, log flash552-clean.log, exit0,
backup/hash/size/protocol checks PASS, only0x2000 and0x200000 written.
Read-only Windows host552 confirms192.168.1.29/WLAN/interface23.

Network command: S31_TCP_PROC_DIAG=1 S31_TCP_ARP_MODE=cold
S31_TEST_BSSID=60:ce:41:ab:02:d0 R/.venv-nuttx/bin/python -u
D/network-repeat.py network553-clean-cold tcp-diag.
Credentials hidden/in memory. No S31_EXPECT_PROTOCOL: committed source does
not contain that temporary runtime print. Offline actual ELF protocol guard
above remains enforced, but negotiated PHY is not asserted in this control.
No changed timeouts, router/TUN/firewall settings, or Windows admin capture.

network553 completed exit1, batch[1,1,1]. All three rounds DHCP and cold/warm
gateway ping PASS; all three TCP connect TimeoutError. Peer ARP absent after
the first failure, TCP state02/SYN_RCVD. This reproduces the fault without
the six temporary source edits; it does not prove all timing effects absent.

host554 read-only MAC verification confirms A8-E2-91-97-1C-B4/interface23 Up.
network554 command uses the same network-probe.py tcp-diag, but
S31_TCP_ARP_MODE=static S31_TCP_PEER_MAC=a8:e2:91:97:1c:b4, other environment
as above. Log network554-clean-static-redacted.log, exit0: DHCP/gateway PASS,
TCP4096bytes echo/hash PASS, server exits. Temporary board RAM ARP entry is
read back before the client starts and cleared by cleanup reset. One static
trial in this group, not a three-round result. No persistent mapping deployed.

Interpretation: address resolution is a reproducible obstacle to the tested
TCP path even in committed code. Static mapping bypasses it. Does not locate
the loss at AP, PC, HAL or over-the-air, nor prove Clash/TUN culpable. HE is
still a separate unaccepted profile; not tested in this group.

Restoration: bash D/flash-demo-pair.sh D/build547-restore-demo.sha256,
flash555-restore-demo.log exit0. PD receipt547 still verifies; no PD rebuild.
Demo556 command N/tools/espressif/esp32s31_demo.py --port /dev/ttyUSB0
--log D/logs/demo556-restore-redacted.log, exit0, PID12,
http://192.168.1.60:8080/. HTTP557 Windows examples/s31demo/test_http.py
192.168.1.60 --samples10 --boundaries (actual CLI: --samples 10), exit0.
Full command in tool history; results in http557-restore.log. Board remains
connected running Demo. No active build/flash/serial/HTTP session.
host557 existing test_esp_rmt_initialize.py exit0; this tests lower-half
cleanup, not the remaining driver-install lifecycle defect described below.
Scripts bash -n and main git diff --check PASS. No new production commit.
checkpoint557.sh seals this group's logs, clean551 firmware/profile,
main diagnostic diff, worktree list and links the verified checkpoint550.

Next safe implementation work while packet-capture authorization is pending:
RMT installation failure propagation and rollback. Read actual rmt_driver_install
and rmt_isr_register: circbuf_init return ignored; ISR registration error
does not prevent module reset and unconditional OK; RX item allocation
failure destroys only rx_sem and leaves the object/buffers allocated. Existing
lower-half initializer cleanup tests stub install and cannot detect these.
Need extracted actual install-function allocation/IRQ failure tests, retry
and existing-channel preservation checks before a small production patch.
Do not mix this with the larger TX ownership/completion redesign yet.
CONFIG_SPIRAM_USE_MALLOC legacy branch has allocator/initialization caveats
documented earlier; handle allocator pairing explicitly, not blindly free.
No RMT production source changes made in this group.
