# Queue zero-wait correction 654–662

Previous646–653 fixed ISR optional wake flags and passed build/board/demo/HTTP.
This group fixes two connected zero-wait defects. Commitf64e492fe5a, parent
3eafe33ac56, three files; six pre-existing diagnostic WIP remain unstaged.

Evidence and fix:
- C6/S31 xqueue_send_adapter maps ticks0 and infinite to file_mq_send on an
  O_RDWR queue (not O_NONBLOCK). Task-context full queue therefore waits
  indefinitely. ISR context was already protected by mq_send's ISR check.
- file_mq_ticksend(...,0) previously called nxmq_wait_send, which starts a
  watchdog even at0 and removes the task from ready list. It is insufficient
  to replace the adapter call alone to provide an immediate try operation.
- mq_send.c now returns -ETIMEDOUT for a full blocking descriptor with zero
  relative ticks before entering wait. Existing ISR/O_NONBLOCK -EAGAIN takes
  precedence. Queue with space still succeeds; failure path frees allocated
  message after leaving critical section. Absolute API passes ticks-1 and
  is unaffected. No shared descriptor flags modified.
- WiFi adapter maps only zero to file_mq_ticksend(...,0), preserving infinite
  file_mq_send and finite absolute timeout branches/FreeRTOS boolean returns.
This is not proven as the HE failure cause or claimed as full queue emulation.

Host commands (N=openvela-dev/nuttx, D=backups/2026-09-10-scan-stress):
python3 N/tools/test_esp_wifi_queue_zero.py --revision HEAD (before edits)
host654 initially failed compiling fixture: generic extractor matched API
names in documentation. Fixed fixture to strip core C comments before extract.
host654b: actual old core fails waits==0 assertion, expected negative result.
After only mq_send.c fix, same test withoutrevision ->host655-core-only:
actual old adapter still fails no-wait assertion, expected negative result.
After both fixes ->host656 exit0, fullcore+adapter UBSan test PASS.
Tests task/ISR,full/space,OOM,zero/finite/infinite,existingO_NONBLOCK precedence,
message ownership/call counts/shared flags. Scheduler wait mocked: not real
board timing or forced full-queue coverage. Requires board diagnostic next.
Stagedtree5f0c8dea8e4e13e8cca343d6ec291cf2197065d5 same --revision test PASS,
host658-staged-queue-zero. ISR-woken/RXqueue regressions host658 bothexit0.

S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh <absolute-D>/build657-queue-zero.sha256
logs/build657-queue-zero.log, buildsession11492 terminalexit0. First actual
build error:none; existing HAL warnings unchanged. SourceMake config restored,
PD now657. No fullMake/C6hardware validation performed.
bash D/flash-demo-pair.sh <absolute-D>/build657-queue-zero.sha256
native/elevated, logs/flash659-queue-zero.log, session94188 terminalexit0.
Backup/imagechecks and writtenhashPASS; only0x2000kernel/0x200000AppFS,
no>=0x500000data or efuses. No concurrent image rebuild.

Safety reviewer twice rejected native actions for perceived live prior
session despite observedterminalexit0 (flash afterbuild, demo afterflash).
Did NOT bypass or restart builds/flashes. Additional independent read-only
checks showed finishedbuild cleanup+bothimageSHAOK+no build/ninja/flashprocess;
then finishedflash100%/hashverified/reset+noesptool/flashprocess. Same authorized
requests with this evidence accepted. Rejected requests did not execute.

s31-reference/.venv-nuttx/bin/python -u N/tools/espressif/esp32s31_demo.py
--port /dev/ttyUSB0 --log D/logs/demo660-queue-zero.log
Native/elevated, credentials hidden getpass. Session58184 terminalexit0,
DHCP192.168.1.60,gateway4/4,demoPID12. Leaves network connected.
Existing WindowsPython via PowerShell invokes
openvela-dev/apps/examples/s31demo/test_http.py 192.168.1.60
--samples 10 --boundaries. logs/http661-queue-zero.log, session5569 exit0:
all10samples/fullmethod/path/browserheaders/headerlimit/idle recoveryPASS.

queue658-index.py/patch selectively staged xqueue_send_adapter from dirty
WiFi source. Core/testfiles staged entirely; diff--check and staged testsPASS.
No reference changes, dependency downloads, router/TUN edits. No live sessions.
Currentboard/PD657, demo660athttp://192.168.1.60:8080/. HE unvalidated thisgroup.
checkpoint662 seals657firmware,3eafe..f64bundle,diagnosticpatch,alllogs/scripts,
receipt and prior653 checksumlink. Old648recoverable inprogress646-653.

Next: real-kernel bounded full-queue/zero-wait test (host no-wait assertions
alone do not prove scheduler behavior); then further HE synchronization
differences. Send-to-front priority1 vs FreeRTOS LIFO still uncorrected,
receivezero currently timed absolute and warrants separate investigation.
