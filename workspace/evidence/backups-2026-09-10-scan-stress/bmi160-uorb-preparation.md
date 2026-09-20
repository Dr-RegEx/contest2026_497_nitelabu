# BMI160 uORB category preparation — TARGET NOT RUN

Prepared September 15 for original published category case **4.1.119 Sensor framework**. This is separate from the common `cmocka_driver_i2c_spi` character test and its image887. It does not close a category gap until the physical BMI160 is connected and the original subscription sequence passes.

## Sources and wiring

Original requirements: `openvela-dev/docs/zh-cn/test_dev_guide/openvela_xts_test_cases.md`, section4.1.119. Actual topic metadata: `apps/system/uorb/sensor/accel.c`, `gyro.c` and `topics.c`. Listener accounting: `apps/system/uorb/listener.c`.

Reuse the wiring already verified against the official board schematic in `bmi160-fixture-preparation.md`: GPIO45/J2-15 SCL, GPIO46/J2-16 SDA, 3.3V VDD/VDDIO, common ground, CSB fixed to3.3V and SDO grounded to select0x68. SDA/SCL need pull-ups to3.3V. GPIO47/48 are reserved for the separate GPIO fixture. No wiring, flashing or serial commands have been performed by this agent.

## Integration

- New board file `src/esp32s31_bmi160_uorb.c`, function `esp32s31_bmi160_uorb_initialize()` under `CONFIG_ESP32S31_XTS_BMI160_UORB`.
- New profile `configs/xts-flat-bmi160-uorb/defconfig`, inheriting `xts-flat-rtc`, with I2C0 pins45/46, I2C_DRIVER, SENSORS_BMI160_I2C, SENSORS_BMI160_UORB, address0x68, USENSOR, UORB, UORB_LISTENER and DEBUG_UORB. Keep `ESP32S31_XTS_BMI160` off: the character binding is a different ABI.
- Main agent owns shared Kconfig/CMake/Make/header/bringup and build/flash integration. Required board dependency: FLAT, I2C0, I2C_DRIVER and SENSORS_BMI160_UORB. The existing uORB registration API publishes both topics with instance0.
- Missing CHIP_ID is returned as an error rather than DEBUGASSERT/panic. If the second registration fails, the first is unregistered and freed on successful cleanup. The optional board binding retains its I2C reference on failure, avoiding disabling a bus referenced by any lower half whose unregister could fail.

## Necessary driver changes and limits

Only `drivers/sensors/bmi160_uorb.c` is changed for this task. Both workers check `bmi160_getregs` and publish no event on an I2C read failure. Axis bytes are decoded explicitly, event structs initialized, and timestamps use the existing `sensor_get_timestamp()` monotonic microsecond API. This removes the previous uninitialized fourth timestamp byte, invalid right shift and mismatched hardware-counter units/clock domain. Work is requeued at a minimum of one tick. Registration propagates errors instead of asserting or masking the first failure.

The prior raw axis-count values are preserved; physical-unit conversion/calibration and temperature measurement are outside this bounded subscription preparation. Zero-initialized unused fields must not be interpreted as measured temperature or calibrated bias. On-hardware rate stability, repeated subscribe/unsubscribe behavior and electrical operation remain unverified.

Source-only check: both actual worker function bodies and the actual event structure declarations were extracted into a temporary C harness. `cc -Wall -Wextra -Werror -fsanitize=address,undefined` passed signed decoding, zeroed fields, monotonic-microsecond timestamps, no publication on bus errors and periodic requeue. The temporary harness was not added as another test suite. Full firmware compilation remains the main agent's next step; host success is not target xTS PASS.

## Original physical test sequence

After the main agent builds and releases the board from long test864, flash the receipt-checked uORB fixture image. Require the boot registration marker and no I2C errors. The two actual topic names are `sensor_accel_uncal0` and `sensor_gyro_uncal0`.

Run this exact three-command sequence, then repeat the whole sequence until **10 rounds** have been recorded (30 listener invocations):

```text
uorb_listener -n 10 -r 25 sensor_accel_uncal0,sensor_gyro_uncal0
uorb_listener -n 10 -r 50 sensor_accel_uncal0,sensor_gyro_uncal0
uorb_listener -n 10 -r 100 sensor_accel_uncal0,sensor_gyro_uncal0
```

Preserve every invocation and raw UART output. Require no crash, no failed subscription, no timeout, both topics receiving data in each invocation, and `Total number of received Message:10/10`. The listener's `-n10` counts **ten messages in total across the topic list**, not ten per topic; inspect both per-topic `recieved:` counters rather than claiming ten each. This follows the original topic-list test; do not replace it with the character test or a custom benchmark. The original document distinguishes ten rounds for individual/vendor acceptance from1000 for community acceptance.
