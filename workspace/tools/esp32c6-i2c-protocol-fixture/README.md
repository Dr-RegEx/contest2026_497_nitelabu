# ESP32-C6 I2C protocol fixture

This ESP-IDF project makes an ESP32-C6 DevKitM act as a generic I2C slave for
the S31 I2C master. It responds at address `0x28`, preloads a deterministic
32-byte payload and its NuttX-compatible CRC32, and logs each write transaction.
The S31 image already contains `cmocka_driver_i2cdev_master`, which exercises
the read and write protocol without requiring a BMI160 sensor.

The default C6 pins are GPIO4 (SCL) and GPIO5 (SDA); change them with
`CONFIG_I2C_SLAVE_SCL_GPIO` and `CONFIG_I2C_SLAVE_SDA_GPIO` if the board wiring
uses different exposed pins. Use 3.3 V logic and common ground. The fixture
enables the C6 internal pull-ups for diagnosis; add external 2.2--4.7 kOhm
pull-ups on both bus lines if the bus is unreliable. Connect those two C6
signals to S31 GPIO45/SCL and GPIO46/SDA. Do not connect the C6 to 5 V.

Build from this directory with the locked ESP-IDF tree:

```sh
source /path/to/esp-idf/export.sh
idf.py set-target esp32c6
idf.py build
idf.py flash monitor
```
