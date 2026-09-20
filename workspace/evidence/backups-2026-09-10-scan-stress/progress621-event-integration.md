# Event groups: task cleanup and kernel integration621–631

Previous goal turn progressed: new but unwired eventgroup candidate and
host tests passed; task-deletion cleanup identified as integration prerequisite.
Start HEAD f65fe96c6d2, Apps ae0dfd89c, board609/demo612; priorWIP preserved.

Reviewed actual sched/task/task_terminate.c, sched/sched/sched_releasetcb.c,
and semaphore/sem_recover.c: task is stopped and semaphore wait registration
recovered before up_release_stack; kernelstack/addrenv release occurs later.
Existing S31 wrapper is therefore the pre-stack-release cleanup entrypoint.
The new hook does not wake a stopped task or touch a releasedstack.

Implementation replaces separate pergroup state locks with one adapterlock
protecting all eventstate and activewaiterregistry, avoiding registry/group
ABBA. A blocked waiter registers its actualTCB and groupref under thatlock.
Normalreturn destroyssem/removesregistry/ref beforeunlock; forcedtaskcleanup
does same for matchingTCB, including already-notified/nonresumedwaiters.
Lastref uses kmm_delayfree while still protected; no unlocked lastref/free
gap in which forcedtaskexit can leak object. Groupowner still must exclude
newcalls after deletion. Reservedhighbyte/any/all/snapshots/timeout semantics
from617 preserved. No PIDcache or kernelpointer in userTLS.

S31 CONFIG_BUILD_KERNEL only: Make adds eventsource beside wifi_task; CMake
adds eventsource underWIFI+KERNEL; actual five sharedadapterwrappers forward
to S31 module under ARCH_CHIP_ESP32S31+BUILD_KERNEL. Otherchips/nonkernel
stubs unchanged. Existing stackrelease wrapper calls eventcleanup before
acquiring its perthreadsemaphore registrylock and before realstackrelease.

Commands/results (D=thisdir, N=workspace/openvela-dev/nuttx):
python3 N/tools/test_esp32s31_wifi_event.py
  host621 exit0: new taskcleanup fourphase hostmodel.
python3 D/check-event619.py
  host622-target-event-syntax.log exit0, actualtargetflags, firsterror:none.
python3 N/tools/test_esp32s31_wifi_event.py
  host624-event-real-hook.log exit0: now combines actual eventmodule AND
  actualwifi_taskmodule/__wrap_up_release_stack with pthread-backed primitives.
  Four forcedstops: pending,set-notresumed,timeout-notresumed,groupdeleted.
  ASan/UBSan/LSan native runs;50rounds two-waiter snapshots and prior617 tests.
python3 N/tools/test_esp32s31_wifi_task.py
  host624-task-regression.log exit0:1000reuse, hookordering/TCB identity.
python3 N/tools/test_esp32s31_wifi_event.py --without-exit-cleanup
  host625-event-no-cleanup.log exit1expected: temporaryfixture removes actual
  hookcall; allocation/semremaining assertion fails after stoppedwaiter.
  No production mutation or sanitizer disabling.
python3 N/tools/test_esp32s31_wifi_heap_callbacks.py
  host628-heap-regression.log4testsPASS.
python3 N/tools/test_esp_wifi_irq_mask.py
  host628-mask-regression.logbothvariantsPASS.
nxstyle event.c/event.h, git diff --check PASS.

S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh D/build623-wifi-event.sha256
> D/logs/build623-wifi-event.log 2>&1
Exit0, firstrealbuilderror:none;1680booleans/protocol7, fullcommands logged,
existingHALwarnings retained. This is CMake targetbuild; Make source-list
updated but no fresh Makebuild thisgroup. Source Makeconfig restored.
bash D/flash-demo-pair.sh D/build623-wifi-event.sha256
> D/logs/flash626-wifi-event.log 2>&1
Exit0, backup+hash+actualELF+sizeguards; only0x2000kernel/0x200000AppFS writes.
No changes toreference/HAL/IDF,unknown dependencies,router,TUN,firewall,efuses
or data>=0x500000. Completed flashbefore serialtests.

S31_EXPECT_PROTOCOL=7 S31_TEST_BSSID=60:ce:41:ab:02:d0
R/.venv-nuttx/bin/python -u D/network-repeat.py network627-wifi-event tcp-udp-dns
(R=workspace/s31-reference). Exit0, BATCH_RESULTS=[0,0,0], allthree rounds
scan/DHCP/gateway/DNS/TCP4096/UDP256x96bytes/normalifdown-reset PASS.
Hiddencredentials, existingclienttimeouts/noARPworkarounds.
This is network+ordinarytaskexit regression, not forcedevent-wait-task-delete
on realhardware. Host forcedstopmodel covers actualhook but mocksNuttXtaskstop/
semrecovery. Targeted board event/SMP/taskdelete stress remains needed.

objdump actual623 event_group_create_wrapper is j esp32s31_wifi_event_create,
not __assert (host628-event-linked.log). OSIaudit629 exit0:128slots/512bytes,
version9/magic, allnonnullslots resolvefunctions. Diagnostic script profile
label changed fromhardcodednuttx609 to nuttx-current; archived614logs unchanged.
Sharedadapter staging patch generated from HEAD by replacing only fiveactual
functionbodies plus guardedinclude; event627-index.py/patch retained.
No existing diagnostic hunks staged. Total9files committed, six oldWIP
remain outsideindex. NewboardREADME explains scope/testcommands and limits.
HE rootcause/status unchanged; does not claim fixing HE by implementing an
unobservedcallback path. No HE diagnosticflash thisgroup.

Commit15cb4c646f3: risc-v/esp32s31: implement kernel Wi-Fi event groups.
Demo command R/.venv-nuttx/bin/python -u
N/tools/espressif/esp32s31_demo.py --port /dev/ttyUSB0
--log D/logs/demo630-event-groups.log: exit0,192.168.1.60:8080 staysconnected.
Windows PowerShell with existingcachedPython runs Apps/examples/s31demo/
test_http.py 192.168.1.60 --samples10 --boundaries (CLI uses separate
--samples 10 arguments), logs/http631-event-groups.log exit0. Ten samples
and method/path/browser-headers/header-limit/idle-recovery boundaries PASS.
checkpoint631 seals623firmware, commitbundlef65..15cb, diagnosticpatch/logs/
scripts and prior620SHA link. CurrentPD623 matchesbuild623-wifi-event.sha256;
boarddemo630 remainsconnected, no activebuild/flash/serial/HTTPprocess.
Next: dedicated actualkernel eventwait/set/timeout/taskdelete regression on
board, including crossCPU scheduling, then furtherHE differential work. Do
not equate normalnetworkpasses with eventcallback paths being exercised.
