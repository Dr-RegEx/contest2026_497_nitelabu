# ADC954 — analog power sequence, BUILD ONLY

Replaces952 with the missing reference analog power/clock preparation:
ANALOG_CLOCK_ENABLE plus SAR internal I2C peripheral enable before sampling,
paired with peripheral disable and ANALOG_CLOCK_DISABLE on close. The root
analog clock uses the existing HAL reference count. SAR internal bus control
is exclusive to this isolated no-Wi-Fi/no-BLE profile.

Build954 passed, 330128bytes. All other952 limitations remain: actual raw
ADC1 CH5/GPIO47 data only, no calibrated millivolt mapping, no hardware sample
or PMU category PASS. See checkpoint952-adc-raw.md for the original command.
952 receipt now points to its frozen image;954 is the active-output receipt.
No UART, reset, flash or physical wiring was touched. Longrun864 continues.
