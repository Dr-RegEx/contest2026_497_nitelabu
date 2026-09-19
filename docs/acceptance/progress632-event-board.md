# Real-kernel event-group validation632–640

Previous turn progressed:15cb4c646f3 eventgroups/kernel taskexit integration,
hostrealhook and network6273/3/HTTP631 passed. Start board623/demo630, Apps
ae0dfd89c. Existing sixdiagnosticWIP preserved, no reference/dependencychanges.

New opt-in boardKconfig ESP32S31_WIFI_EVENT_TEST requiresS31SMP+WIFI+KERNEL,
defaultoff; demo-rmt-event includesdemo-rmt and enablesonlythisbool. Board
Make/CMake compile esp32s31_event_test.c onlyinprofile. Bringup executes test
before startingWiFi. Production module exposes locked activewaitercount only
for this diagnostic. Normalbuild excludes testsource/countercode.

Real board test uses one gated kthread on oppositeCPU, actualS31event APIs
and actual kthread_delete/up_release_stack cleanup. Parent affinity saved,
set toCPU0thenCPU1, restored atfinish. Worker staysparked after result so
parent never races auto-reap/PIDreuse. Gates allow setting worker affinity
before eventwait; confirms actualsched_getcpu onbothsides.
Modes eachdirection:0set/fullsnapshotclear,1partialtimeout2ticks,
2deleteeventwithblockedwaiter,3force-deleteblockedtaskthenqueryliveevent.
Non-timeout modes require TCBstateWAIT_SEM notstart/parksem AND exactlyone
registered eventwaiter. Eachfinish requires registryempty and expectedbits.
Cleanup deletes worker before destroying sharedsems/event; ifdeletefails,
stops test and preservesstatic jobresources ratherthan freeingliveobjects.
Bounded parent waits; no pin oruserstorageaccess. Does not prove every
forcedkill instructionwindow or long-term heap/SMPstress; hostgated races
from621–631 remain complementary evidence.

Commands (D=thisdir, R=workspace/s31-reference, N=workspace/openvela-dev/nuttx):
S31_DEMO_PROFILE=demo-rmt-event bash D/build-demo.sh D/build632-event-board.sha256
> D/logs/build632-event-board.log 2>&1
Exit1, firstrealerror: implicitdeclaration enter_critical_section in newboard
test; leave_critical_section similarly. Current declaration is in
include/nuttx/spinlock.h, not just irq.h. Added correctinclude, no publiccode
override. Nofailedimageflashed.
Same command with build632b-event-board.sha256/log exit0,1681booleans/ELF7,
firstrealerror:none; existingHALwarnings retained. Fullcommandsinlogs.
Archive firmware632b-event-board.tar.gz before rebuildingPD.
bash D/flash-demo-pair.sh D/build632b-event-board.sha256
> D/logs/flash633-event-board.log 2>&1 exit0, backup/hash/size/ELFguardsPASS.
Only0x2000kernel/0x200000AppFS, no>=0x500000data orsecurityfuses touched.

R/.venv-nuttx/bin/python -u D/event634-boot.py
> D/logs/serial634-event-board.log 2>&1
Exit0,3resets24casesPASS. Everycase CPUdirection/mode/ret checked inoutput,
not just summary. NormalNSH/SMP/MTDboundary/WiFiready markers and ifdownPASS.
Reusable tool added N/tools/espressif/esp32s31_event_smoke.py:
R/.venv-nuttx/bin/python -u N/tools/espressif/esp32s31_event_smoke.py
--port /dev/ttyUSB0 --boots 3 > D/logs/serial635-event-repo-smoke.log 2>&1
Exit0, second3resets24casesPASS. Combined6resets48cases, notlongsoak.
Parser unit test R/.venv-nuttx/bin/python N/tools/test_esp32s31_event_smoke.py
> D/logs/host636-event-parser.log 2>&1 exit0:rejectmissing/duplicate/wrongCPU/
wrongreturn/reversedorder/fatal/mixedFAILorcleanupfailure transcripts.

S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh D/build635-event-restored.sha256
> D/logs/build635-event-restored.log 2>&1 exit0, firsterror:none,ELF7. Thisbuild
started onlyafter flash633exit0; board tests consume632b ondevice, notPDfiles.
Confirmed CONFIG_ESP32S31_WIFI_EVENT_TEST off; sourceMakeconfig restored.
bash D/flash-demo-pair.sh D/build635-event-restored.sha256
> D/logs/flash637-event-restored.log 2>&1 exit0, afterserial635exit0.

Commit8fb1462ee68: cross-core event-group board regression,11files. Scoped
code/config/tool/READMEchanges only; sixpre-existingWIP remain unstaged.
No fullMake rebuild thisgroup; Makeintegration listed but CMake was built.
README records boundedboardcoverage and command, notHEacceptance.
R/.venv-nuttx/bin/python -u N/tools/espressif/esp32s31_demo.py
--port /dev/ttyUSB0 --log D/logs/demo638-event-restored.log exit0,
PID12 at http://192.168.1.60:8080/, noEVENT_TEST markers in ordinaryboot.
Windows existingPython invokes Apps/examples/s31demo/test_http.py
192.168.1.60 --samples 10 --boundaries, logs/http639-event-restored.log exit0,
10samples/fullmethod/path/browserheaders/headerlimit/idlerecovery PASS.
checkpoint640 seals632b/635firmware,15cb..8fb commitbundle,diagnosticpatch,
logs/scripts and prior631SHA link; SHA256SUMS-progress640 checked.
Noactivebuild/flash/serial/HTTPprocess. Board/PDcurrent635, old623PDreceipt
no longer matches regeneratedfiles. Demo638 remainsconnected.
No HEfix claimed here; nextworkreturns toHE data-path differentiation.
