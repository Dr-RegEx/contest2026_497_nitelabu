# BMI160 fixture preparation — TARGET NOT RUN

Prepared September 15 for the user's September 17 peripheral session. No sensor is assumed attached. This is an independent FLAT test image; the running SMP/KASAN long test and the LAN demo image remain separate. Original `cmocka_driver_i2c_spi` is unchanged.

## Wiring

The official [ESP32-S31 Function-CoreBoard-1 V1.0 schematic, sheet 2](https://dl.espressif.com/schematics/esp32-s31-function-coreboard-1-schematics.pdf) was downloaded and its J2 region visually inspected. GPIO45 and GPIO46 connect to J2 pins 15 and 16 and have no other sheet connections. GPIO47/48, J2 pins 13/14, are reserved for the separate GPIO loopback fixture. Header pin numbers are schematic pin numbers; locate the board's pin-1 marker before connecting.

| Board connection | BMI160 module connection |
| --- | --- |
| J2 pin 35 or 36, 3.3 V | VDD/VCC and VDDIO if separately exposed |
| J2 pin 33 or 34, GND | GND/GNDIO |
| J2 pin 15, GPIO45 | SCL / SCx |
| J2 pin 16, GPIO46 | SDA / SDx |
| 3.3 V | CS / CSB, held high from power-up |
| GND | SDO / address-select, selecting 0x68 |

Use a BMI160 breakout with 3.3 V supply and logic, and common ground. SDA and SCL require external pull-ups to 3.3 V; use the module's fitted pull-ups, or approximately 4.7 kΩ each for short fixture wires. Do not connect these signals to J2's 5 V pins. INT1/INT2 are not needed for this polling test. These connections follow the [Bosch BMI160 datasheet, sections 3.2.1 and 3.2.3](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi160-ds000.pdf). Connect with power off, after the main agent releases the board from its long test.

## Firmware integration

New board file: `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_bmi160.c`.

Initialization: `int esp32s31_bmi160_initialize(void)`, guarded by board option `CONFIG_ESP32S31_XTS_BMI160`. It initializes I2C0, calls the existing `bmi160_register("/dev/accel0", i2c)`, logs success only after registration, and releases its I2C reference on failure. Missing/wrong CHIP_ID produces `-ENODEV` and no `/dev/accel0`. The main agent owns the Kconfig, CMake/Make, header and bringup integration.

Profile: `configs/xts-flat-bmi160/defconfig`, inheriting `xts-flat-rtc`. Required settings: I2C0 with SCL45/SDA46, SENSORS, SENSORS_BMI160, SENSORS_BMI160_I2C, BMI160_I2C_ADDR_68. SENSORS_BMI160_UORB is disabled deliberately: the actual original test reads `struct accel_gyro_st_s` from `/dev/accel0`; it does not subscribe to uORB. The published checklist's general USENSOR/UORB instructions do not change that source ABI. This covers its permitted I2C variant, not an additional SPI adaptation. I2C0 is remapped away from onboard codec pins 50/51 only in this fixture image.

## Necessary existing-driver corrections

- `bmi160_base.c`: compile the SPI transfer helper only in SPI configurations; return I2C errors from `bmi160_getregs`.
- `bmi160_base.h`: matching internal `int bmi160_getregs` prototype. Existing uORB callers may ignore the return as before; their behavior is otherwise unchanged.
- `bmi160.c`: propagate the read error instead of falsely returning a full record, decode little-endian axes and all three sensor-time bytes without consuming an uninitialized fourth byte, reject short buffers and return exactly one record, propagate `register_driver` failure.

Host check `check-bmi160-host.py` extracts and compiles the actual `bmi160_getregs`, `bmi160_read`, and `bmi160_register` function bodies with a bus shim. ASan/UBSan and `-Wall -Wextra -Werror` pass signed-axis/24-bit-time decoding, short/oversized buffer behavior, I2C error propagation, absent CHIP_ID, registration failure and success. Evidence: `logs/bmi160-fixture-host.log`. It does not establish I2C electrical compatibility or target xTS PASS. No firmware build or UART operations were run by this sub-agent.

## Physical acceptance after firmware build

1. Flash the main agent's paired, receipt-checked fixture image after the long test finishes and connect the fixture as above.
2. Require the boot line `xTS BMI160: /dev/accel0 I2C0 addr=0x68 SCL=45 SDA=46` and no I2C transfer errors.
3. Run `cmocka_driver_i2c_spi` and retain the complete raw UART log. The original case requires 100 complete reads and one cmocka test PASS. Move the module gently during the sample capture to provide visible changing six-axis data; this is supporting evidence, not a modification of the test's assertions.
4. Record the actual module, wiring, firmware receipt, test output and result. Missing hardware remains NOT RUN; a successful host check or build is never target PASS.
