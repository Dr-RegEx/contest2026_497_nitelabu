# Active peripheral integration contract

User authorized parallel agents September 15, 18:37.
Main agent owns shared board Kconfig, CMakeLists.txt, Make.defs, board header, bringup, all build/flash helpers and main ledger. No agent may access the UART, reset or flash the board. The main agent alone schedules firmware builds; active test PID144782 must continue.

GPIO agent: own new board esp32s31_gpio.c only; use ESP32S31_XTS_GPIO and esp32s31_gpio_initialize(void). GPIO47 input (/dev/gpio0), GPIO48 output (/dev/gpio1), J2 pins13/14. Keep original xTS unchanged. Supply real interrupt evidence since its poll timeout assertion is insufficient.

BMI160 agent: own new board esp32s31_bmi160.c and new xts-flat-bmi160 profile. Use ESP32S31_XTS_BMI160 and esp32s31_bmi160_initialize(void). Verify spare header pins distinct from GPIO47/48, prefer45/46 if schematic confirms free. Main agent authorizes necessary minimal fixes in drivers/sensors/bmi160_base.c, bmi160_base.h and bmi160.c (I2C error propagation, correct24-bit timestamp decoding, register error return, SPI-only helper conditional compilation). Inspect users of changed internal prototype; do not rewrite unrelated uORB/SPI paths or original tests. Add bounded host evidence for these real bugs if feasible without firmware build. Report profile/pins and registration absence behavior.

Main integration update 18:45: GPIO build886 PASS. BMI160 profile now includes I2C_DRIVER=y (required by board binding Kconfig and existing i2c_register). Main is ready to build BMI160 once agent confirms source edits finalized; host tests may continue in parallel.

Final integration: GPIO agent delivered esp32s31_xts_gpio.c and esp32s31_xts_gpio_initialize(). Main retained those names, mapped GPIO47 to /dev/gpio0 and GPIO48 to /dev/gpio1 consistently with the profile/guide, and added IRQ counters outside ISR logging. Both pads initialize as inputs. Build886 passed; BMI160887 passed. Both sub-agent assignments finished without UART access. The main agent reviewed actual driver changes and archived source888; target tests remain pending.
