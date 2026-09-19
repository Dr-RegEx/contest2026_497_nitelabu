# Watchdog candidate999 after the original996 failure

996 is a real failed original test run and must remain recorded as FAIL.
Original mode0 reached drivertest_watchdog.c:353 after its final busy wait,
without the expected watchdog panic/reset. The log contains no measurement
of that wait; inaccurate generic busy delay is a supported hypothesis, not a
measured fact. Frozen868 uses CONFIG_BOARD_LOOPSPERMSEC=15000 and a320 MHz
CPU configuration. The generic delay explicitly requires loop calibration.

The independently built xts-flat-wdt-romdelay profile adds only the default-off
ESP32S31_WDT_ROM_DELAY option to the original watchdog profile. Its up_udelay
and up_mdelay use existing esp_rom_delay_us, with maximum1000us calls to avoid
large cycle-count multiplication. The locked clock HAL already updates ROM
ticks/us on CPU frequency changes. No HAL/IDF or original testcase was changed.
Other profiles retain their prior generic delay implementation.

One500ms busy-delay diagnostic runs during watchdog initialization, measures
monotonic OS ticks, prints requested/measured milliseconds and rejects outside
450..550ms. This is a prerequisite diagnostic; it does not replace original
mode3 status/timing assertions or prove all delay ranges on hardware.

A separate definite driver error was fixed: wdt_hal_handle_intr feeds before
clearing the interrupt. The fatal ISR now invokes existing rwdt_ll_clear_intr_status
without feeding, preserving stage1's hardware reset deadline. The capture path
still explicitly feeds before invoking the capture callback. Start/stop/capture
configuration retain their existing HAL reset/rearm behavior. Actual linked
code was inspected: the fatal branch contains no feed call; only the handler
branch feeds, and the ROM delay is linked at2f80003c.

Host result: independent clean build, source whitespace, shell syntax and F0
lock checks passed. The868 original image receipt still verifies. Candidate999
is not a target PASS until the unmodified watchdog modes0/1/2/3 pass. Do not
retry or reset between failed modes to manufacture a sequence. Root owns UART;
this preparation performed no UART/reset/flash/provisioning action.
