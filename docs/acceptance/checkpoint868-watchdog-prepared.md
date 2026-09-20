# S31 watchdog integration: compiled candidate, target validation pending

The board remains on longrun864. No reset or flash occurred for this work.

`xts-flat-wdt` is a separate single-core M-mode FLAT profile for the unchanged
watchdog application, which directly masks interrupts and enters an ISR loop.
The new RTC-WDT lower half registers `/dev/watchdog0`; timeout, start/stop,
feed, status and capture use the locked S31 HAL. Stage0 is an interrupt;
without a capture handler, stage1 performs a real RTC-system reset after a
five-second panic-dump allowance. `BOARD_RESET_ON_ASSERT=0` prevents a software
reset from replacing the hardware reset reason. Board cause mapping preserves
the distinction between system/core/CPU watchdog resets.

The profile enables the existing RISC-V CLIC threshold and nested IRQ support.
Normal critical sections mask levels1..6 with MINTTHRESH0xdf; fatal watchdog
delivery uses level7. Capture callbacks switch to level1 because they may call
normal kernel APIs. `up_irq_enable` now handles the CLIC threshold when that
configuration is enabled; the existing SMP demo does not enable this path.

866 failed on missing declarations.867 built but post-link inspection found
the public chip IRQ header had been regenerated from its unchanged canonical
source, leaving the original interrupt-mask instruction at0xff.868 updates
the canonical header and builds successfully. `logs/host868-wdt-threshold.log`
confirms the unchanged mode1 application writes223/0xdf to CSR0x347.

**None of the four watchdog modes has run on the board.** Priority preemption,
normal capture delivery, actual timeout/reset timing and panic stacks remain
unverified. This is not SMP-watchdog acceptance. The current xTS count does not
increase based on compilation/disassembly.

After longrun864 completes and releases UART:

```sh
S31_FLAT_PROFILE=xts-flat-wdt bash backups/2026-09-10-scan-stress/flash-xts-flat.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build868-flat-wdt-threshold.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-watchdog.py --receipt backups/2026-09-10-scan-stress/build868-flat-wdt-threshold.sha256
```

Log each command to a fresh evidence file. The runner resets once initially
then runs0/1/2/3 in order. It requires a watchdog-specific panic, actual register
and stack output, hardware reset reason0x10 and normal NSH reboot for0/1/2.
Mode3 must complete original feed/status/capture/stop with no reset. There are
no automatic recovery resets or retries to hide failures.

The image/config/symbols are in `firmware868-flat-wdt-build-only.tar.gz`, hashed
by `SHA256SUMS-xts868`. Scripts pass syntax checks and source diff checks pass.
