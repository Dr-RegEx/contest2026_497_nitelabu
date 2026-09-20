# ARP queue safety and cold-ARP comparison preparation

Prior goal turn made progress (three RMT commits, hardware regression and
sealed checkpoint505). This group returned to the unresolved ARP/TCP path.
Basic demo506 on503 was running before the planned diagnostic flash515.

NuttX new commit1fee91b2d2a: both arp_out ARP-queue handoff branches now free
the original IOB chain if arp_queue_iob returns an error, then clear device
pointers AND d_len. netdev_iob_release alone does not reset d_len. The old
code cleared pointers unconditionally, losing ownership on ENOMEM/ENOENT.
This feature was OFF in previous failures, so this leak is not their cause.

host507: test_arp_queue_ownership.py --revision 21d0fd73f0c
old runtime ownership assertion failure. host508 fixed two extracted actual
handoff blocks PASS: initial/in-progress, success/ENOMEM/ENOENT, no leak or
double release. nxstyle and git diff --check PASS. host510 ARP parser,
SYN-ACK parser and TCP accept address-context tests also PASS.

build509: S31_DEMO_PROFILE=demo-rmt-arp-queue bash D/build-demo.sh
D/build509-arp-queue.sha256 (D is the absolute parent of this note).
Exit0, first real build error:none. New diagnostic config explicitly keeps
NET_ARP_SEND=y while enabling NET_ARP_SEND_QUEUE=y; otherwise Kconfig's
default would disable active ARP send. NET_ARP_IPIN and11AX remain OFF,
NET_STATISTICS ON. Five retries/20ms and other timeouts unchanged.
509 was NOT flashed: review found expiry worker frees the table queue
without the network lock while arp_queue_iob can append concurrently.
IOB queue link manipulation is not internally serialized.

Second source change (now committed as cc3da0ce101): shared
g_arp_queue_lock protects enqueue and expiry detachment; expiry frees a
local detached queue outside the lock. Other table replacement/update/
cleanup paths already synchronize the worker with work_cancel_sync and
are network-locked. No network lock added inside expiry, avoiding a
cancel_sync/network-lock deadlock. No private lock held during IOB freeing.

host511 old code fails shared-lock assertion. host512 actual extracted
arp_unreach_work/arp_queue_iob with deterministic handoff model PASS:
detached queue free, enqueue during free preserved, errors/work pending.
This model is not a full real-thread SMP stress test or proof of every
work-queue lifecycle. nxstyle/diff-check PASS.
build513 same command/profile with build513-arp-expiry.sha256, exit0,
first real build error:none. host514 Windows read-only address check:
192.168.1.29, WLAN, interface23. No privileged capture or network changes.

515 flash: flash-demo-pair.sh build513 receipt, exit0,only0x2000/0x200000.
516 cold-ARP TCP repeat: S31_EXPECT_PROTOCOL=7,
S31_TCP_PROC_DIAG=1, S31_TCP_ARP_MODE=cold,
S31_TEST_BSSID=60:ce:41:ab:02:d0, network-repeat.py
network516-arp-queue-cold tcp-diag. Credentials hidden; no static mapping,
IP harvesting, timeout increases or HE. Actual result:[1,1,1], process exit1.
All DHCP/cold+warm gateway checks PASS, all TCP connect attempts TimeoutError.
Each round submits one42-byte peer ARP request, HAL submission returns0,
SYN-ACK seen remains0. No new peer ARP reply across each TCP attempt;
rounds2/3 receive other ARP requests, so total RX is not completely silent.
Driver submission success is not proof of over-air transmission or PC receipt.
Cannot attribute the loss to HAL, radio, AP, PC or TUN yet.

Archived firmware513 before rebuild. config478-to513.diff proves the .config
diff from the earlier no-queue ARP trace is exactly NET_ARP_SEND_QUEUE=y;
source also includes intervening RMT fixes and the two queue safety fixes,
so do not describe the whole firmware as a single-source-change experiment.

build517: same build-demo.sh, profile demo-rmt and receipt
build517-demo-arp-fixes.sha256, exit0, first real error:none. This checks the
queue-disabled build and prepares restoration; network test still ran513.
518 restore flash completed exit0, hash verified, same two offsets.
Restored517 config confirms ARP_SEND_QUEUE, ARP_IPIN and11AX all OFF.
Demo519 startup exit0, PID12 at192.168.1.60:8080; HTTP520 exit0,100 samples
and method/path/browser-header/header-limit/idle recovery PASS. Board remains
connected with Demo running. Host520 common suite and F0 lock verifier PASS
(nested HAL namespace audit476 still applies).
Experimental queue config remains untracked, not adopted as production.
Two safety fixes remain independently justified by fault/ownership tests.

ARP queue expiry is100ms at current config. It preserves a packet awaiting
resolution but does not itself add ARP retransmissions; therefore it may
not resolve the observed missing/late peer ARP response. Do not adopt as
production or claim Wi-Fi fixed merely because this candidate builds.
