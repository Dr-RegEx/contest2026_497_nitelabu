# iperf2 delay backend artifact audit — 2026-09-20

## Correction of the earlier source audit

The previous claim that these candidates use the Kalman delay backend was incorrect. The earlier search found HAVE_KALMAN/HAVE_NANOSLEEP but missed the higher-priority HAVE_CLOCK_NANOSLEEP branch. The source was not recently changed to enable this backend: the external repository's HEAD already defines HAVE_CLOCK_NANOSLEEP when signals are enabled. The current config.h diff concerns HAVE_MLOCKALL, not delay selection. `apps/external/iperf2` resolves into the separate `external` repository.

No Kalman patch was prepared or applied. Its static state and conversion concerns do not explain these candidates' measured UDP TX rates because the active delay_loop does not call it.

## Three actual object files agree

Existing target objects for `esp32s31-wifi-rx-observe`, `esp32s31-wifi-ap-selection`, and `esp32s31-wifi-window64` are byte-identical:

`e3965cb9b53ad7bb1e6a0773fe58d811eb062d4e6fbbaa5bb428a70ccfd4f583`

Each `delay_loop` disassembly has a `R_RISCV_CALL_PLT clock_nanosleep` relocation at offset 0x30. The argument setup supplies clock ID 1 (CLOCK_MONOTONIC), flags 0 (relative sleep), a timespec on the stack, and NULL remaining-time pointer. It has no call to delay_kalman.

Each Ninja dependency record points to the common source header `apps/external/iperf2/config.h` and its own output directory's `include/nuttx/config.h`. CMake's iperf2 definition uses `-DHAVE_CONFIG_H` and the checked-in source include directory; there is no generated per-profile iperf2 config.h in this path. Therefore the evidence supports a mistaken prior audit, not profile contamination.

Preserved evidence includes `*-delay-disassembly.txt`, `*-delay-dependencies.txt`, `*-delay-compile-command.txt`, and [artifact-hashes.json](artifact-hashes.json). Commands used only existing objects and Ninja metadata:

- `riscv32-esp-elf-objdump -dr <out>/apps/external/iperf2/CMakeFiles/iperf2.dir/iperf2/compat/delay.c.o`
- `ninja -C <out> -t deps apps/external/iperf2/CMakeFiles/iperf2.dir/iperf2/compat/delay.c.o`
- `ninja -C <out> -t commands apps/external/iperf2/CMakeFiles/iperf2.dir/iperf2/compat/delay.c.o`

## Active pacing semantics

`Client.cpp:1087` calculates its UDP inter-packet target in nanoseconds. `RunUDP()` subtracts observed loop duration using microsecond packet timestamps multiplied by 1000, then maintains a running delay debt. At line 1218, delay/1000 converts to microseconds for delay_loop. The direct backend converts that relative duration into seconds/nanoseconds and calls CLOCK_MONOTONIC with flags 0. This is not an absolute timestamp accidentally passed as a relative duration. The separate `clock_usleep_abstime()` correctly uses CLOCK_REALTIME plus TIMER_ABSTIME for realtime-based absolute deadlines; it is not the UDP per-packet delay call.

Current NuttX uses a 1 ms periodic tick, without high-resolution timer timekeeping in these profiles. `clock_time2ticks()` rounds the relative timespec up to ticks, then `clock_delay2abstick()` adds one tick when scheduling the watchdog in `nxsig_clockwait()`. Ignoring dispatch latency, a 280 us request therefore waits approximately 1–2 ms; a 2243 us request approximately 3–4 ms depending on phase. Dispatch may add more. These are genuine coarse-timer costs, not a nanosecond conversion error.

However, RunUDP's running-delay accounting skips subsequent sleeps when the producer is behind schedule. It resets an excessively negative debt at the send-timeout bound (500 ms with `-i 1`), rather than insisting on one sleep for every packet. Consequently there is no demonstrated fixed 3.7 Mbps ceiling solely from these sleep durations. CLOCK_REALTIME measurements versus monotonic sleeps could react to a wall-clock adjustment, but the audited evidence does not show such an adjustment. Signal interruption can shorten a relative sleep; the same pacing feedback accounts for observed elapsed time.

The direct backend's `usec * 1000` expression can overflow 32-bit unsigned long for unusually long requests above approximately 4.295 seconds. The observed UDP targets (280/2243 us) and Reporter request (16000 us) are far below this range; this is not a demonstrated cause of the current measurement and no unrelated patch is proposed.

## Result and scope

No clear active-backend timing defect has been established for the observed runs. The 1 ms timer resolution is a performance constraint, but changing to absolute waits or bypassing pacing would require measured justification and must not alter the requested test load. The ongoing controlled socket-window comparison is a better next discriminator than modifying an unused Kalman backend.

No source/configuration edits, compilation, image build, serial access, or board operations were performed in this artifact audit. Only evidence files in this directory were created.
