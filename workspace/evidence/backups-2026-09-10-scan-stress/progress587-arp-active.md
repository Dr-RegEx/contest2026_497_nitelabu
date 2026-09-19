# Active ARP control587–594 and callback failure cleanup

Previous group made progress:92e89d75e91 RMT TX lifecycle committed and
firmware582/LED584/Demo585/HTTP586 validated; checkpoint586 sealed. Current
main starts92e89d75e91, Apps ae0dfd89c; six diagnostic WIP files preserved.

Read-only audit: NET_ARP_GRATUITOUS is incoming ARP learning, not an outgoing
announcement switch, so no gratuitous option was changed. arp_out issues the
passive TCP response's first ARP request; icmp_sendmsg invokes active arp_send
with configured retries. Prior135 had peer ping failures, but not this exact
cold-map readback/active-resolution/TCP control. Initial mistaken reads of
netdev_notify.c and arp_wait.c got ENOENT; actual waiter functions were read
from net/arp/arp_notify.c. No guessed content used.

network-probe.py adds opt-in S31_TCP_ARP_MODE=resolve (other modes unchanged):
clear the board peer map, REQUIRE lookup reports absent, ping-c1 peer using
unchanged timeout, read map immediately, then existing TCP4096 test and reset.
No static mapping, client timeout extension, router/TUN/firewall change.
Python compile check PASS. host587 read-only Windows confirms192.168.1.29,
WLAN/interface23. Hashes of clean551 and mainPD582 both verified before test.

flash588 uses existing flash-clean551.sh, exit0, verified old551 from detached
committed39be74d0093 without diagnostic source edits. No download/rebuild.
This is the same firmware as clean553 cold3/3 failures and554 static success.
Only existing two authorized flash ranges. Main PD582 images stay intact.

network589 command: S31_TCP_PROC_DIAG=1 S31_TCP_ARP_MODE=resolve
S31_TEST_BSSID=60:ce:41:ab:02:d0 R/.venv-nuttx/bin/python -u
D/network-repeat.py network589-clean-active-arp tcp-diag.
Hidden credentials, no S31_EXPECT_PROTOCOL because clean source lacks runtime
print; flash verified actualELF7. All3 DHCP/gateway PASS. All3 active peer
lookups ABSENT, ICMP FAIL, ~1.86–1.89s inclusive probe/readback. Each logs
three arp_wait -ETIMEDOUT messages despite configMAXTRIES5/DELAY20ms; do not
claim all5 actual packet submissions occurred. arp_find can mark unreachable
after MAXTRIES*DELAY in-progress interval. TCP resultsFAIL/FAIL/PASS; round3
has peer MAC mapping by post-TCP read. Exit1/batch[1,1,0]. Active pre-resolution
does not stably fix it. No capture means AP/PC/driver loss location remains
unknown; no basis to blame Clash/TUN. Clean test has no TX request counters.

Independent source bug found during arp_send audit: after arp_wait_setup,
arp_callback_alloc failure breaks without canceling the queued waiter; later
mempool_release leaves a dangling waiter. Added one arp_wait_cancel(notify)
before the-ENOMEM break. New test_arp_send_callback_failure.py extracts the
actual failure block and actual wait_setup/cancel/notify functions. Models
state release with free; oldHEAD triggers ASan heap-use-after-free in notify,
host590-before exit1; fixed test exit0, preserves another waiter and supports
retry. It does not execute full arp_send or real mempool scheduling. This is
OOM-path hardening, not proven explanation of the observed Wi-Fi failure.
Scoped nxstyle and git diff --check PASS.

Build591 command: S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh
D/build591-arp-callback.sha256. Full commands in build591-arp-callback.log,
exit0, first real build error:none, actualconfig/ELF7. D=this note's directory.
Flash592 exit0. Demo593 exit0,PID12 at192.168.1.60:8080. HTTP594 exit0,
10samples and full boundary suite PASS; host594 prior ARP queue ownership/
expiry regressions PASS. Commitf10a8f4b209 contains only net/arp/arp_send.c
and tools/test_arp_send_callback_failure.py. devif_callback_free was also
read and confirmed null callback cleanup is guarded by if(cb).
checkpoint594.sh seals firmware591, commit bundle92e89d75e91..HEAD, logs,
updated diagnostic script and prior586 SHA link. Six prior diagnostic WIP
files remain unchanged. No active build/flash/serial/HTTP session; Demo593
stays connected. PD is591, so older582 PD receipt is no longer current.

Next potential non-invasive control: exact clean551 code/profile with only
ARP_SEND_DELAYMSEC changed in a separate diagnostic profile/build directory.
Reason: current in-progress window is5*20ms=100ms and replies sometimes arrive
later; test whether a longer ARP resolution window changes map availability.
Do NOT extend TCP client timeout or deploy the diagnostic config as a fix;
measure map readbacks and end-to-end results against unchanged baseline.
This remains a hypothesis, not proof of Wi-Fi power-save/AP delay. Preserve
original551 artifacts and restore basicDemo afterward. Admin PC capture
authorization still unanswered; no admin capture attempted.
