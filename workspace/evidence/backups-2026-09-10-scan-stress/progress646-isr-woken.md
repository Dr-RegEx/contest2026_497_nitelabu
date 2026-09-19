# ISR optional wake flag fix 646–653

Previous643–645 made progress: actual HR affinity checked, old CPU0-only
failure cross-checked, demo644 and HTTP645 passed. This group fixes a proven
OSI callback defect, not a claimed HE root cause.

Commit3eafe33ac56 (parent8fb1462ee68), three files:
- arch/risc-v/src/esp32c6/esp_wifi_adapter.c queue_send_from_isr_wrapper
- arch/risc-v/src/esp32c6/esp_coex_adapter.c two semaphore-from-ISR wrappers
- tools/test_esp_wifi_isr_woken.py
These shared callbacks are used by C6/S31. They unconditionally wrote0 to
hptw. Locked IDF FreeRTOS queue.c xQueueGenericSendFromISR accepts NULL and
only sets pdTRUE when needed, never clears an earlier wake request. NuttX
mq_sndinternal.c nxmq_notify_send and sem_post.c perform ready-list insertion
and nxsched_switch themselves; no separate caller-controlled ISR yield flag
is needed here. Adapters now ignore the optional flag without dereferencing
or clearing it; underlying queue/sem calls and returns are unchanged.

Tests (all paths relative workspace; D=backups/2026-09-10-scan-stress):
python3 openvela-dev/nuttx/tools/test_esp_wifi_isr_woken.py --revision HEAD
before edits: exit1, logs/host646-isr-woken-before.log, first real error UBSan
store to null pointer of type int in actual extracted queue callback.
Same test without --revision after edits: exit0, host647-isr-woken-after.log.
Checks actual three callback definitions with downstream mocks, NULL and
false/true flags, both success/failure, exact pointer/tick/priority forwarding
and once-only calls. Not real ISR scheduling or evidence of HAL passingNULL
during HE failure. Staged tree b56fc5a0f512ad52dc14412aea57d2102a008322 also
passed --revision test, logs/host650-staged-isr-woken.log.
python3 tools/test_esp_wifi_rx_queue.py and test_esp_wifi_irq_mask.py from
NuttX: exit0, logs/host649-rx-queue.log and host649-irq-mask.log. Host UBSan.

S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh <absolute-D>/build648-isr-woken.sha256
Output logs/build648-isr-woken.log, exit0, full commands recorded, first real
build error:none. Existing locked HAL shadow/GPIO format/PHY endif warnings
remain. Source Make config restored; current CMake PD is now648, not635.
No full Make or C6 hardware build/validation this group.

bash D/flash-demo-pair.sh <absolute-D>/build648-isr-woken.sha256
native/elevated, logs/flash650-isr-woken.log, exit0. Backup/receipt checks and
written hashes passed. Only0x2000kernel/0x200000AppFS; no>=0x500000 or efuses.
No build mutates consumed images during flash. Serial starts only afterexit0.

s31-reference/.venv-nuttx/bin/python -u
openvela-dev/nuttx/tools/espressif/esp32s31_demo.py --port /dev/ttyUSB0
--log D/logs/demo651-isr-woken.log
native/elevated; authorized credentials via hidden getpass. Exit0, boot,
DHCP/gateway and demoPID12 at http://192.168.1.60:8080/ passed.
Existing Windows Python via PowerShell runs
openvela-dev/apps/examples/s31demo/test_http.py 192.168.1.60 --samples10
(actual CLI uses '--samples 10') --boundaries.
logs/http652-isr-woken.log exit0, all10samples/fullboundariesPASS.

Selective index patch D/isr650-index.py/patch stages only queue callback in
dirty WiFi adapter; other two files staged in full after inspection. Staged
diff --check passed. Six pre-existing tracked diagnostics remain unstaged,
untracked old profiles/tests preserved. No reference/dependency/router/TUN
changes, downloads, or new authorization needed.

checkpoint653.sh seals648ELF/bin/AppFS/config, scoped commitbundle, WIP patch,
logs/scripts/receipts and prior640+645 checksum links. Board and PD648; demo651
remains connected. No live build/flash/serial/HTTP sessions. FullHE still open.

Next concrete interface audit: xqueue_send_adapter currently maps ticks0 and
infinite wait to file_mq_send. mq_send.c treats ISR as nonblocking but task
context may block on a full queue. Also send-to-front is mapped to priority1,
which may differ from FreeRTOS front insertion ordering. These are hypotheses
to reproduce against actual functions, not changes included in this commit.
