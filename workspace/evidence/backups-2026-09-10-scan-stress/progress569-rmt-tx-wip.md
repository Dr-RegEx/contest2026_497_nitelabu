# RMT TX lifecycle work in progress569–573

Previous turn was progress: RX recursive lock fix b40bb54c514, host/build567
PASS, checkpoint568 sealed. Main HEAD still b40bb54c514; Apps ae0dfd89c.
Board remains build559, Demo562 running (not rechecked physically this group).
No flash/reset/serial/network tests this group. Prior firmware559 archived564.

Current UNCOMMITTED changes beyond six old diagnostic files:
- common/espressif/esp_rmt.c: tx_lock mutex separates writer ownership from
  completion semaphore(initial0), tx_state_lock serializes transfer state.
  Lock order tx_state_lock then common register spinlock. IRQ never takes
  writer mutex. New rmt_tx_finish_nolock disables DONE/THRES/ERROR/LOOP sources,
  stops/resets/acknowledges hardware, clears data pointers and active state.
  New rmt_tx_interrupt consumes per-channel TX events before posting completion;
  error wins over DONE/LOOP, threshold refill serialized with cancellation.
  write_items uses uninterruptible wait, explicitly retires on negative return,
  drains a raced completion post before unlocking writer mutex. Wrapper makes
  a kernel snapshot and frees it only after synchronous return. TX_ERROR
  interrupt now explicitly enabled in rmt_tx_start.
- tools/test_esp_rmt_tx_lifecycle.py new, untracked: actual helper/start/write
  functions, deterministic mocked event schedules and lock-order assertions.
- test_esp_rmt_acquire.py routes new source to lifecycle test, old source still
  has old fixture. test_esp_rmt_install.py models mutex cleanup/spin init.
  test_esp_rmt_write.py checks snapshot independence/contents/OOM/free.

This is NOT yet a production-ready or fully verified TX fix. Do not commit
or flash until remaining checks below. Other six diagnostic WIP untouched.
One generated apply_patch hunk initially failed context before any changes;
subsequent patches verified with diff. A declaration briefly landed in read
instead of write and was corrected before build. No reference changes.

host569 initial lifecycle fixture compile error: misleading indentation in a
one-line for loop. Fixed braces; retry exit0 with ASan/UBSan/LeakSanitizer.
host570 aggregate install/write/acquire/initialize/RX tests exit0; scoped
nxstyle and git diff --check PASS. Actual build571 exit1: first real error
MIN implicitly declared; also SP_UNLOCKED is a struct initializer, not a
valid assignment expression in this NuttX. Replaced MIN by ternary and used
spin_lock_init API; updated host install fixture. Build572 exit0, actual
config/ELF guard PASS7. host573 regression exit0 after those corrections.

Full build commands logged:
S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh
D/build571-rmt-tx-lifecycle.sha256 (failed; no receipt)
S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh
D/build572-rmt-tx-lifecycle.sha256 (success).
D is this note's directory. Source Make config restored. PD now572, older
PD receipts567/559 invalid. No live build or host-test session remains.

Required next work before commit/flash:
1. Genuine two-writer scheduler/threads test. Current lifecycle fixture uses
   mutex semantics/lock failure checks, NOT concurrent pthread writers. Do not
   claim concurrent/SMP verified from its single-thread event schedule.
2. Coherent IRQ status snapshot audit. Current helper calls separate DONE,
   ERROR, LOOP,THRES channel-mask getters under software locks; hardware can
   change between reads. Consider a single volatile32-bit register snapshot
   using rmt_ll_get_interrupt_status_reg (verify availability in other current
   common-driver targets), then decode RMT_LL_EVENT_* masks. S31 HAL's
   rmt_ll_tx_get_interrupt_status excludes ERROR, so do not use it blindly.
3. Explicit synchronous private contract: sole real rmt_write_items caller
   passes true, but wait_tx_done now only asserted. Decide whether remove
   obsolete private argument or returnEINVAL forfalse; do not imply async
   support is still valid. Loop-end now stops hardware before return; verify
   finite/infinite loop implications and document API scope.
4. Run sanitizer tests again, target build, then guarded paired flash and
   bounded GPIO60 RGB + basic HTTP regression. Test long kernel-buffer refill
   with safe onboard waveform only after reviewing whether extra pixels reset
   or simply shift offchain; no arbitrary new pin or high-current waveform.
5. Fault injection cancellation/error and concurrency must be bounded, no
   hanging raw device write exposed as completed. Cancellation escape stops
   hardware under locks, but abrupt task deletion and infinite TX without
   completion/IRQ timeout remain to audit.

No new commit this group. Backup checkpoint573 includes exact patch, all
untracked files/tests/profiles, firmware572 and logs chained to checkpoint568.
Broad goal remains active; Wi-Fi HE/coldARP still not accepted; pending
Windows admin capture approval not answered and not acted on.
