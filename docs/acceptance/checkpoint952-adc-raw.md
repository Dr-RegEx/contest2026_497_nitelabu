# ADC candidate952 — raw sampling, BUILD ONLY

Dedicated xts-flat-adc profile registers /dev/adc0 on ADC1 channel5, GPIO47,
J2 pin13. Pin mapping comes from the locked S31 adc_channel.h and the board
schematic. Registration does not start conversion; open disables digital
input/output and both pulls, selects the XTAL clock and enables ADC1.
The lower half uses the pinned S31 LL directly, polls a real DONE flag for
at most10000us, clears/disables the trigger and forwards the actual sample.
Timeout is an error, never a zero sample. Close disables this isolated ADC.
Wi-Fi/BLE/SMP and the GPIO loop driver are excluded from this profile.

Original application command (after864 and physical setup):

    adc -p /dev/adc0 -n 20

The original example is unchanged, uses software triggering and one sample
per group. Its error path logs failed triggers then attempts a blocking read;
a failed hardware trigger may therefore require terminating the application
from the host timeout handler. Do not treat this as a successful20-sample run.

The datasheet v0.5 describes12-bit SAR hardware; the locked S31 LL exposes a
17-bit result field. This port preserves raw result bits and does NOT claim
17-bit analog accuracy or calibrated millivolts. The locked adc_cali_schemes.h
has no enabled S31 calibration scheme. Expected-voltage acceptance requires
an actual stable voltage/GND fixture and a justified raw-to-voltage mapping;
raw command success alone does not pass the PMU ADC category. Whether the
PMU-specific heading maps to the on-chip SAR also remains explicit.

Build952 passed,329872bytes, archive manifest verified. Device registration
and adc_main are linked. No ADC hardware sample has been obtained. The actual
board is still running864. Flash helper permits this isolated profile only
with ADC/application/symbol guards and its existing UART ownership guard.

Superseded for target execution by954, which adds analog root clock/SAR internal bus preparation. See checkpoint954-adc-power.md and use build954-flat-adc.sha256. All raw-data/calibration limitations remain.
