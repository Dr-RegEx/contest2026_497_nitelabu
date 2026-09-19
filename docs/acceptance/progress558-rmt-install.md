# RMT installation rollback558 onward

Previous turn made progress: clean committed-source control reproduced cold
ARP TCP failure3/3 and static mapping passed1/1. Demo547 restored and
checkpoint557 sealed. Main NuttX started39be74d0093 with six diagnostic edits
preserved; Apps ae0dfd89c. This group changes only esp_rmt.c and adds a test.

rmt_driver_install now propagates circbuf allocation, ISR registration and
ISR mutex acquisition errors. Failure destroys both initialized semaphores,
uninitializes the circular buffer, frees RX pingpong/object with matching
allocator, clears only the failed channel pointer, and returns the error.
No module enable/reset follows failed installation. Existing installed channel
is retained. Legacy SPIRAM branch initializes semaphores and records its
actual interrupt allocation flags, previously zero/uninitialized in that path.
Current S31 does not enable this legacy option; host variants exercise it.
Actual libs/libc/semaphore/sem_init.c returnsOK for valid nonnull objects,
so no artificial semaphore-init failure contract was invented here.

New test_esp_rmt_install.py extracts the real function. Four variants cover
SPIRAM on/off and RX pingpong on/off, TX/RX, regular/IRAM allocation, each
allocation failure, IRQ/lock errors, retry, duplicate rejection and preserving
another installed channel. Tracks allocation-family pairing and semaphore
lifetime with ASan/UBSan/LeakSanitizer. Not concurrent init, realIRQ or TX
completion testing. Initial permission review timed out before executing;
one permitted retry ran exit0 (host558-rmt-install.log). Old HEAD test exits1
at expected IRQ-error assertion (host558-rmt-install-before.log), demonstrating
the regression. Scoped nxstyle and git diff --check PASS.

Build command: S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh
D/build559-rmt-install.sha256. D is this note's directory. Full commands in
logs/build559-rmt-install.log; exit0, first real build error:none. Config/ELF
protocol guard PASS7. Main Make configuration restored by script.
Do not use older PD receipts547 after this rebuild; prior firmware preserved
in checkpoint550. Isolated clean551 still independent and unmodified.

flash560: bash D/flash-demo-pair.sh D/build559-rmt-install.sha256, exit0.
led561: R/.venv-nuttx/bin/python -u
N/tools/espressif/esp32s31_led_smoke.py --port /dev/ttyUSB0 --cycles 3,
exit0,12frames,brightness8,finaloff,boot/timer/console PASS. This run is not
new optical confirmation; earlier user-confirmed sequence533 remains valid
historical evidence, not invalidated by script's optical=unverified label.
host560 existing write/acquire/initialize regression tests all exit0.
Demo562 started PID12 at http://192.168.1.60:8080/, exit0; stays connected.
HTTP563 Windows test_http.py192.168.1.60 --samples 10 --boundaries exit0,
10samples and method/path/browser-headers/header-limit/idle-recovery PASS.
Permission review yielded before launching then completed; no duplicate run.
Commit6186228f55c: risc-v/espressif: unwind failed RMT driver installation,
only esp_rmt.c and tools/test_esp_rmt_install.py. Six temporary diagnostic
tracked files and existing untracked profiles/tests remain untouched.
checkpoint564.sh archives firmware559, commit bundle39be74d0093..HEAD,
logs and progress with SHA verification chained to checkpoint557.
No active build/flash/serial/HTTP session; Demo562 remains running.

Remaining: concurrent same-channel initialization still lacks a transaction
lock around publication; rmt_config also runs before install. TX ownership,
completion/cancellation/error wakeup, raw user-buffer lifetime and full RX
are distinct larger debts. Do not claim this patch resolves them. Wi-Fi HE
and cold ARP remain unaccepted; Windows admin capture permission unanswered.
