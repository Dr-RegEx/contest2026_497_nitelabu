# Event-group candidate617–620 (not wired/deployed)

Previous goal turn progressed: linked OSI audit614–616 identified five actual
eventgroup panic stubs, identical128slotABI, and ruled out naive nxevent API
forwarding. Start/main HEAD f65fe96c6d2, Apps ae0dfd89c; board/PD609/demo612.

New untracked candidates:
- N/arch/risc-v/src/esp32s31/esp32s31_wifi_event.c
- N/arch/risc-v/src/esp32s31/esp32s31_wifi_event.h
- N/tools/test_esp32s31_wifi_event.py
N=workspace/openvela-dev/nuttx. Existing six diagnostic WIP unchanged.
No Make/CMake/callback-table integration yet. No firmware build or flash, no
new commit. Do not claim that eventgroups are now available on the board.

Candidate semantics:24applicationbits (highbyteFreeRTOScontrolreserved),
fullpreclear snapshots on immediate waits, partialbits on timeout, waitany/
all, clearonexit, wakeallmatchingwaitersbeforeapplyingunionofclears. Each
waiter holds objectref until it has stopped accessingeventlock. Groupdelete
wakes pendingwaiterswith0; already latched setresults survive later deletion.
Finite tickwait andUINT32MAXindefinitewait, kernelheap allocation/OOM handling.
Normal owner must prevent newcalls once group deletion begins.

Tests use entire actual candidate source with pthread mutex/cond-backed
NuttXprimitive shims, allocation/sem counts and ASan/UBSan/LSan.50rounds of
two concurrentwaiters test samepreclear snapshot; deterministic resume gates
cover set→delete beforewaiterresumes, timeout→set race, deletepending and
timedout-not-resumedwaiters. Emptyset, partialtimeout, clear, OOM checks too.
Hostshims are not actual NuttXscheduler/IRQ/SMP/taskdelete certification.

Commands (D=thisdirectory), logs:
python3 N/tools/test_esp32s31_wifi_event.py
  host617-wifi-event.log exit1: LeakSanitizer fatal under ptrace sandbox,
  not a target build error. Re-ran unchanged outside sandbox, did NOT disable
  sanitizer: host618-wifi-event.log exit0, S31_WIFI_EVENT=PASS.
N/tools/nxstyle <candidate.c or .h>: initially missingsectionwarnings,
  added NuttX IncludedFiles/Types/Functions headings; both exit0 afterward.
git diff --check PASS.
python3 D/check-event619.py > D/logs/host619-target-event-syntax.log 2>&1
  exit0. Reuses actual609 esp32s31_wifi_task compilerflags, substitutes only
  candidate source, strips object/dependencyoutputflags, forces correctPD
  config.h and adds-fsyntax-only. Full command logged; first real compiler
  error:none. This is target syntax/typecheck, NOT a linked firmware build.
One discovery miss sem_waituninterruptible.c; actual files found withrg:
sem_wait.c,sem_tickwait.c,etc. No guessed source contents used.

Critical integration prerequisite found during review: candidate has stack
waiternodes. Forced deletion of a blocked task must unlink them before stack
release; groupdelete reference handling alone is insufficient. Therefore do
NOT wire thesecallbacks orcommit as completed until task-exit cleanup exists.
Existing S31 __wrap_up_release_stack in esp32s31_wifi_task.c already runs for
normalexit/cancel/failedsetup and frees perTCBsemaphore withkmm_delayfree.
Next implement waiterregistration/cleanup via this hook, with lockordering,
notified-but-not-resumed cases and delayedfree under schedulercriticalstate.
Add forcedtaskdelete tests (pending/set/timeout races) beforeintegration.
Consider a single adapterlock guarding registry+groupstate to avoid an ABBA
between groupdelete andtaskcleanup. This is a designcandidate, not yet code.
Existing hook only linked forWIFI+BUILD_KERNEL; otherbuildmodes need explicit
handling instead of silently claiming forcedtaskdelete coverage.

No reference/IDF/HAL/router/TUN/firewall/credential changes. Board retains
working609demo; no active build/flash/serial/client session at handoff.
checkpoint620.sh seals allthreeWIPfiles, scripts/logs/status and prior616SHA.
Goal remains full adaptation; eventgroup module still incomplete and HE
failure remains unproven with respect to these presently unused callbacks.
