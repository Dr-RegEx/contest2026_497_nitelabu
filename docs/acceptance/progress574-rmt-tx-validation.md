# RMT TX lifecycle validation574 onward

Previous turn made progress: uncommitted lifecycle implementation plus host
tests/build572; checkpoint573 sealed. Main HEAD b40bb54c514, Apps ae0dfd89c.
Six old diagnostic files preserved. Board initially559/Demo562.

This group reads the TX interrupt status register once under state/common
locks, then decodes DONE/ERROR/THRES/LOOP masks. Verified the register-pointer
LL API exists in locked C3/C6/H2/S31 HAL. Unlike TX_MASK helper, the full
status includes ERROR. Private wait_tx_done=false now returns-EINVAL (the
only real caller passes true); docs describe synchronous completion/abort.

New test_esp_rmt_tx_concurrency.py reuses the actual functions extracted by
the lifecycle fixture, replacing scheduler mocks with pthread-backed writer,
state/register locks and condition-variable completion. 50rounds/100writers,
second blocked while first refills, first DONE succeeds, second ERROR|DONE
returns-EIO, ordered distinct frame contents, exactly two completion posts.
Alarm10seconds/subprocess15seconds bound deadlocks. host574 exit0 with
ASan/UBSan/LeakSanitizer. This is genuine HOST thread testing, not board SMP
stress or an electrical waveform test. Register hardware remains mocked.

host575 install/write/lifecycle/initialize/RX tests exit0. build575 and576
exit0, actual config/ELF protocol guard7. First real build error:none in this
group (earlier571 failures recorded in progress569). Scoped nxstyle caught
one missing blank and then a long comment line; both fixed. Final style
host576a exit0 before final build576a. Full commands logged as:
S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh D/build<id>-rmt-tx-lifecycle.sha256
for ids575,576,576a. D=this note's directory. Main Make config restored.

build576a exit0, flash577 exit0, LED578 exit0(12frames,brightness8,finaloff),
Demo579 PID12 at192.168.1.60:8080 exit0, HTTP58010samples/boundaries exit0.
These are for576a, not the final null-channel addition below.

Reviewing the README's older test_esp_rmt_completion.py found its extracted
ISR-block selector obsolete after the refactor; updated entry to run actual
new lifecycle tests. Also retained old ISR behavior for null-object channels:
disable and acknowledge all its TX sources instead of leaving a pending IRQ.
New shared rmt_tx_event_mask includesERROR; null path avoids object access.
Lifecycle test now covers null-object status cleanup. host581 full README
suite PASS: HAL compat, WS2812 lifetime/write, completion, write, acquire,
initialize, install, RX start, lifecycle,50rounds pthread concurrency.
README lists new tests. host582 scoped style PASS.
build582 final rebuild command uses the same build-demo.sh, profiledemo-rmt,
receipt build582-rmt-tx-lifecycle.sha256. Build582 exit0, config/ELF protocol7,
no real build error. Flash583 exit0 after backup/hash/size checks, only0x2000
and0x200000. LED584 exit0,12frames/brightness8/finaloff,timer/console PASS.
Demo585 exit0,PID12 at192.168.1.60:8080. Optical label remains unverified for
these new runs; earlier user confirmation533 is historical basic-color evidence.
HTTP586 Windows test_http.py192.168.1.60 --samples 10 --boundaries exit0,
10requests and method/path/browser-headers/header-limit/idle-recovery PASS.
Commit92e89d75e91 contains8files: esp_rmt.c, boardREADME, four updated
write/install/acquire/completion tests, two new lifecycle/concurrency tests.
No other staged edits. Six existing diagnostic source files remain WIP.
checkpoint586.sh archives firmware582, bundleb40bb54c514..HEAD, logs and
diagnostic snapshot, with SHA verification chained to573. No active build,
flash,serial or HTTP process from this group; Demo585 stays running.

For further long-frame hardware testing, current board registers only
/dev/leds0 with one pixel; esp_ws2812_write bounds writes to dev->nleds, so
simply asking s31led for more cycles does NOT exercise threshold refill.
Would need a reviewed diagnostic path on the same GPIO60 (e.g. shared raw
lower half with safe encoded frames), bounded lifetime and finaloff. Do not
select arbitrary pins or label present smoke as long-frame coverage.
Do not use earlier PD receipts after final build. Reference/unknown repos,
network settings and flash partitions beyond the two approved ranges untouched.

Residual scope after this fix must stay explicit: long-frame refill/cancel
has host evidence; onboard single-pixel smoke does not establish hardware
long-frame timing, raw task-kill behavior, all loop configurations, RX waveforms,
DMA or universal SMP/MMU safety. No arbitrary RX/TX pins selected. An IRQ
that never arrives can still wait indefinitely; safe bounded timeout design
must account for configured clock and requested frame duration, not an
arbitrary short constant. Six Wi-Fi diagnostics remain WIP, coldARP and HE
not accepted. Windows admin capture authorization remains unanswered.
