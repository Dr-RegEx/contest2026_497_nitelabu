# HR affinity observation and demo recovery 643–645

Previous group641–642 produced effective-config evidence, not just a plan.
This group queried the actual board and restored a verified running demo.
NuttX HEAD remains8fb1462ee68; six tracked diagnostic edits and original
untracked profiles/tests preserved. git diff --check passed. No code changes,
builds, flashing, reference changes, downloads, router/TUN edits or efuses.

Initial sandbox ls /dev/ttyUSB0 and /dev/serial/by-id returned ENOENT. This
was sandbox visibility, not a disconnected board: elevated read-only
PowerShell `usbipd list` showed CP2102N10c4:ea60 BUSID2-1 Attached; elevated
ls showed correct ttyUSB0 and serial-ID symlink. No reattach/bind performed.
Process query showed no overlapping serial/demo/network test process.

Command (native/elevated):
s31-reference/.venv-nuttx/bin/python -u
backups/2026-09-10-scan-stress/timer643-affinity.py
Output logs/serial643-timer-affinity.log. Exit0.
Script sets RTS/DTR false before opening, never calls hard_reset, then uses
uname/ps/proc status/stat read commands. Three samples show hr_timer PID4,
affinity0x3, semaphore-waiting, priority223. WiFi PID5 affinity0x1 priority253.
Waiting CPU '---' does NOT identify callback execution CPU; no claim made.

First command returned a POWERON boot transcript instead of uname response,
and ps contained no demo task. Timing/cause of boot not established; could be
buffered output, external power event, or opening-related behavior. The
script's reset=0 marker means no explicit reset request, not proof that no
hardware reset occurred. Thus prior demo638 continuity was NOT assumed.

Cross-check old actual logs/network201-cpu0-he-1-redacted.log: four masks0x1,
gateway0/4 twice, TCPconnect timeout. README and rounds2/3 likewise record
CPU0-only HE failures. Current0x3 vs IDF-pinnedCPU0 is a genuine difference,
but not a demonstrated HE cause. Do not repeat blind CPU0 restriction.

Recovery command (native/elevated, hidden getpass credentials):
s31-reference/.venv-nuttx/bin/python -u
openvela-dev/nuttx/tools/espressif/esp32s31_demo.py --port /dev/ttyUSB0
--log backups/2026-09-10-scan-stress/logs/demo644-restored.log
Exit0. Existing firmware reset by demo tool, DHCP192.168.1.60, gateway4/4,
PID12 running at http://192.168.1.60:8080/. No flash or storage formatting.

Windows existing cached Python invokes
openvela-dev/apps/examples/s31demo/test_http.py 192.168.1.60
--samples 10 --boundaries via PowerShell, native/elevated.
Output logs/http645-restored.log, exit0. All10samples and method/path/browser
headers/header-limit/idle-recovery boundaries PASS. Board remains connected.
No live test sessions remain. Firmware still last flashed635, no new image.

Next HE work: differential receive synchronization/cache/interrupt interfaces;
timer unit and CPU0 restriction currently lack evidence as root-cause fixes.
Full goal remains incomplete. No commit for an audit-only group. Source and
firmware recovery archive632–640 unchanged; this group's script/note/logs
checksummed separately with prior642 checksum file included.
