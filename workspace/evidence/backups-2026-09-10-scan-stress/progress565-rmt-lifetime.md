# RMT completion evidence and RX lock fix565 onward

Previous turn made concrete progress: committed6186228f55c, installation
rollback host variants/build559/flash560/LED561/Demo562/HTTP563 PASS,
checkpoint564 verified. Main starts at that commit with six diagnostic WIP
files preserved. Apps ae0dfd89c. No live process at start.

probe565-rmt-completion.py extracts actual rmt_write_items. In a modeled
96-item transfer, first ownership wait succeeds and completion wait returns
-EINTR or -ECANCELED. Both produce return0, hardware-active flag true,
48-item tail pointer retained, and ownership semaphore posted. Output
host565-rmt-completion.log exit0 means the existing BUG was reproduced,
not that RMT completion passed. No board transfer/cancellation was attempted.
This shows why merely copying user data then freeing on write return would
introduce a kernel use-after-free while threshold ISR still holds the tail.

Actual include/nuttx/semaphore.h nxsem_wait_uninterruptible retries only
-EINTR, still returns -ECANCELED. Do not replace waits and claim solved.
drivers/rmt/rmtchar.c has a mutex member but neither write nor read locks it;
raw writers can still race for ownership/completion on the same tx_sem.

While auditing lock order, found an independent RX-start deadlock:
rmt_rx_start holds common rmt_spinlock, then calls rmt_set_rx_thr_intr_en
which acquires the same nonrecursive spinlock. S31 SOC_RMT_SUPPORT_RX_PINGPONG
is1 in locked HAL soc_caps.h. Helper has exactly one caller. Renamed it
rmt_set_rx_thr_intr_en_nolock, documented caller ownership, removed only its
inner lock/unlock. RX-start retains the encompassing transaction lock.

New tools/test_esp_rmt_rx_start.py extracts actual helper and rx_start.
Old HEAD fails at !locked assertion, host566-rmt-rx-start-before.log.
Fixed source passes pingpong on/off, both decoded RX channels, reset on/off,
exactly one lock/unlock and expected threshold/enables. Scoped nxstyle and
git diff --check PASS. No RX pin has been selected, no hardware RX test.

Build command: S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh
D/build567-rmt-rx-lock.sha256 (D=this note's parent). Full commands in
logs/build567-rmt-rx-lock.log. Exit0, first real build error:none, artifact
config/ELF guard PASS protocol7. Commit b40bb54c514 contains only esp_rmt.c
and tools/test_esp_rmt_rx_start.py. checkpoint568.sh archives firmware567,
commit bundle6186228f55c..HEAD, diagnostics snapshot and logs, chaining
SHA verification to checkpoint564. No active build/flash/test session.
Board stays on559 with Demo562: this group does not flash or reset it.
PD rebuilt567 invalidates old559 PD receipt; firmware559 archived564.

Next substantial TX lifecycle work must couple ownership, completion and
buffer lifetime, not independently add a snapshot or a success-path lock:
- Per-channel writer mutex distinct from completion semaphore initialized0.
- Per-channel IRQ state spinlock shared by start/abort/TX ISR, documented
  ordering against common register spinlock. Avoid recursive wrapper calls.
- Completion processing must collect/ack same-channel DONE/THRES/LOOP/ERROR
  coherently before waking a waiter; late event must not finish next transfer.
- On completion/error/cancellation: disable only this channel TX sources,
  stop hardware as needed, clear pending events and retained tail, then wake
  or return; no ISR reader may survive buffer release.
- Kernel snapshot must cover raw userspace long frames as well as WS2812.
- Tests: callback-before-wait, two writers, error+done collision, cancellation,
  long-frame refill, stale threshold after completion, failure cleanup.
- Loop/asynchronous support must be audited before changing the private
  wait_tx_done signature (only production caller currently passes true).

HAL lookup: correct component is components/esp_hal_rmt/esp32s31/include/
hal/rmt_ll.h, not components/hal/esp32s31 (one initial read got ENOENT).
TX_MASK macro excludes TX_ERROR; abort masks must explicitly include ERROR.
S31 compatibility header is N/arch/risc-v/src/esp32s31/hal_backports/include/
hal/rmt_ll_compat.h. No reference files changed.

Wi-Fi cold ARP and HE remain separate incomplete work. Admin PC capture
permission unanswered; no capture/TUN/router changes. Keep broad goal active.
