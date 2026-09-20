#!/bin/bash
# Separate offline FLAT test build. Never writes or flashes the main demo.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_profile=${S31_FLAT_PROFILE:-xts-flat}
case "$task_profile" in
  xts-flat|xts-flat-adc|xts-flat-rtc|xts-flat-flash|xts-flat-wdt|xts-flat-gpio|xts-flat-bmi160|xts-flat-category-fs|xts-flat-category-kv|xts-flat-bmi160-uorb|xts-flat-pwm|xts-flat-category-fs-flash|xts-flat-category-fs-perf|xts-flat-category-fs-recovery|xts-flat-flash-raw|xts-flat-audio|xts-flat-ble|xts-flat-bttool|xts-flat-usb-adb) ;;
  *) echo 'Unsupported FLAT profile' >&2; exit 1 ;;
esac
task_build=$task_root/openvela-dev/out/esp32s31-$task_profile
task_receipt=${1:?Pass a fresh absolute build receipt}
test ! -e "$task_receipt"
test -f "$task_root/openvela-dev/apps/testing/cmocka/cmocka/src/cmocka.c"
cd "$task_root/openvela-dev/nuttx"
task_saved=$(mktemp -d /tmp/s31-flat-source-config.XXXXXX)
restore_config() {
  if [ -f "$task_saved/.config" ]; then mv "$task_saved/.config" .config; fi
  if [ -f "$task_saved/config.h" ]; then mv "$task_saved/config.h" include/nuttx/config.h; fi
  rmdir "$task_saved"
}
trap restore_config EXIT
if [ -f .config ]; then mv .config "$task_saved/.config"; fi
if [ -f include/nuttx/config.h ]; then mv include/nuttx/config.h "$task_saved/config.h"; fi
. "$task_root/s31-reference/tmp/esp-idf-clean/export.sh" > "$task_dir/logs/flat-idf-export.log" 2>&1
export PATH="$task_dir/offline-bin:$task_root/s31-reference/.venv-nuttx/bin:$PATH"
export ESP_HAL_3RDPARTY_LOCAL="$task_root/s31-reference/deps/esp-hal-3rdparty"
set -x
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:$task_profile \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" --target resetconfig
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:$task_profile \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
for task_option in ARCH_CHIP_ESP32S31 ESPRESSIF_ESP32S31 BUILD_FLAT TESTING_MM TESTING_CMOCKA TESTING_DRIVER_TEST BCH ESPRESSIF_SPIRAM_USE_8LINE_MODE; do
  rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
done
if rg -q '^CONFIG_BUILD_(KERNEL|PROTECTED)=y$' "$task_build/.config"; then
  echo 'ERROR: FLAT build unexpectedly selected a protected/kernel image' >&2
  exit 1
fi
cmake --build "$task_build" -j8
test -f "$task_build/nuttx.bin"
test "$(stat -c %s "$task_build/nuttx.bin")" -le 2088960
rg -q 'mm_main' "$task_build/System.map"
rg -q 'cmocka_driver_block_main' "$task_build/System.map"
if [ "$task_profile" = xts-flat-rtc ] || [ "$task_profile" = xts-flat-flash ]; then
  for task_option in RTC_DRIVER RTC_ALARM RTC_PERIODIC SIG_EVTHREAD; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q 'cmocka_driver_rtc_main' "$task_build/System.map"
fi
if [ "$task_profile" = xts-flat-flash ]; then
  rg -q '^CONFIG_ESP32S31_XTS_FLASH=y$' "$task_build/.config"
  rg -q 'esp32s31_xts_flash_initialize' "$task_build/System.map"
fi
if [ "$task_profile" = xts-flat-wdt ]; then
  for task_option in ESP32S31_RWDT WATCHDOG BOARDCTL_RESET_CAUSE ARCH_RV_HAVE_CLIC ARCH_HIPRI_INTERRUPT; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q '^CONFIG_BOARD_RESET_ON_ASSERT=0$' "$task_build/.config"
  rg -q 'cmocka_driver_watchdog_main' "$task_build/System.map"
fi
if [ "$task_profile" = xts-flat-gpio ]; then
  for task_option in ESP32S31_XTS_GPIO DEV_GPIO ESPRESSIF_GPIO_IRQ; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q 'esp32s31_xts_gpio_initialize' "$task_build/System.map"
  rg -q 'cmocka_driver_gpio_main' "$task_build/System.map"
fi
if [ "$task_profile" = xts-flat-bmi160 ]; then
  for task_option in ESP32S31_XTS_BMI160 SENSORS_BMI160_I2C I2C_DRIVER; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q '^CONFIG_ESPRESSIF_I2C0_SCLPIN=45$' "$task_build/.config"
  rg -q '^CONFIG_ESPRESSIF_I2C0_SDAPIN=46$' "$task_build/.config"
  if rg -q '^CONFIG_SENSORS_BMI160_UORB=y$' "$task_build/.config"; then
    echo 'ERROR: original sensor test requires the character interface' >&2
    exit 1
  fi
  rg -q 'esp32s31_bmi160_initialize' "$task_build/System.map"
  rg -q 'cmocka_driver_i2c_spi_main' "$task_build/System.map"
fi
if [[ "$task_profile" = xts-flat-category-fs* ]]; then
  for task_option in FS_ROMFS EXAMPLES_ROMFS FS_FAT FAT_LFN_UTF8 TESTING_FATUTF8 FS_TEST_STRESS FS_TEST_STABILITY; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in romfs_main fatutf8_main vela_fs_stability_test01_main performance_test_main; do
    rg -q "$task_symbol" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-category-kv ]; then
  for task_option in ESP32S31_XTS_FLASH FS_LITTLEFS KVDB_SERVER KVDB_UNQLITE KVDB_TEMPORARY_STORAGE CM_KVDB_TEST; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in cmocka_kv_test_main vela_kvdb_stability_test02_main kvdbd_main; do
    rg -q "$task_symbol" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-bmi160-uorb ]; then
  for task_option in ESP32S31_XTS_BMI160_UORB SENSORS_BMI160_UORB UORB UORB_LISTENER I2C_DRIVER; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in esp32s31_bmi160_uorb_initialize bmi160_register_uorb uorb_listener_main; do
    rg -q "$task_symbol" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-pwm ]; then
  rg -q '^CONFIG_ESP32S31_XTS_PWM=y$' "$task_build/.config"
  rg -q 'esp32s31_pwm_setup' "$task_build/System.map"
  rg -q 'cmocka_driver_pwm_main' "$task_build/System.map"
fi
if [ "$task_profile" = xts-flat-category-fs-flash ] || [ "$task_profile" = xts-flat-category-fs-perf ] || [ "$task_profile" = xts-flat-category-fs-recovery ]; then
  for task_option in ESP32S31_XTS_FLASH FS_LITTLEFS FS_TEST_POWEROFF; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in power_off_test01_main power_off_test02_main power_off_test03_main; do
    rg -q "$task_symbol" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-category-fs-perf ]; then
  rg -q '^CONFIG_NSH_CMDOPT_DD_STATS=y$' "$task_build/.config"
  rg -q '^CONFIG_TESTING_TESTCASES_PRIORITY=255$' "$task_build/.config"
fi
if [ "$task_profile" = xts-flat-category-fs-recovery ]; then
  for task_option in ESP32S31_XTS_FLASH FS_LITTLEFS FS_TEST_STABILITY BOARDCTL_RESET; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q '^CONFIG_BOARD_RESET_ON_ASSERT=2$' "$task_build/.config"
  rg -q '^CONFIG_TESTING_TESTCASES_STACKSIZE=32768$' "$task_build/.config"
  for task_symbol in vela_fs_stability_test03_main vela_fs_stability_test04_main; do
    rg -q " [Tt] ${task_symbol}$" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-flash-raw ]; then
  for task_option in ESP32S31_XTS_FLASH ESP32S31_XTS_FLASH_RAW BCH NSH_CMDOPT_DD_STATS; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in ftl_initialize_by_path bchdev_register; do
    rg -q "$task_symbol" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-audio ]; then
  for task_option in ESP32S31_AUDIO ESP32S31_I2S AUDIO_ES8311 ES8311_RATE_DEPENDENT_MCLK AUDIO_DRIVER_SPECIFIC_BUFFERS SYSTEM_NXPLAYER SYSTEM_NXRECORDER SYSTEM_YMODEM; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in esp32s31_audio_initialize esp32s31_i2s_initialize cmocka_driver_audio_main nxplayer_main nxrecorder_main rb_main sb_main; do
    rg -q "$task_symbol" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-ble ] || [ "$task_profile" = xts-flat-bttool ]; then
  for task_option in ESP32S31_BLE DRIVERS_BLUETOOTH UART_BTH4; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  if rg -q '^CONFIG_(SMP|ESPRESSIF_WIFI|ESPRESSIF_SPIRAM_USER_HEAP)=y$' "$task_build/.config"; then
    echo 'ERROR: BLE candidate requires the awake internal-SRAM profile' >&2
    exit 1
  fi
  for task_symbol in esp32s31_ble_initialize esp_bt_controller_init esp_bt_controller_enable uart_bth4_register; do
    rg -q " [Tt] ${task_symbol}$" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-bttool ]; then
  for task_option in BLUETOOTH_TOOLS BLUETOOTH_SERVICE BLUETOOTH_STACK_LE_ZBLUE BT_HCI_HOST; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q 'bttool_main' "$task_build/System.map"
fi
if [ "$task_profile" = xts-flat-usb-adb ]; then
  for task_option in ESP32S31_USBDEV USBDEV USBADB SYSTEM_ADBD ADBD_USB_SERVER ADBD_SHELL_SERVICE PSEUDOTERM FS_BINFS; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in esp32s31_usbinitialize usbdev_register adbd_main; do
    rg -q " [Tt] ${task_symbol}$" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-adc ]; then
  for task_option in ESP32S31_XTS_ADC ADC EXAMPLES_ADC EXAMPLES_ADC_SWTRIG; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q ' [Tt] esp32s31_adc_setup$' "$task_build/System.map"
  rg -q ' [Tt] adc_main$' "$task_build/System.map"
fi
if rg -q '^CONFIG_ESP32S31_XTS_FLASH=y$' "$task_build/.config" &&
   rg -q '^CONFIG_ESPRESSIF_SPIRAM_USER_HEAP=y$' "$task_build/.config"; then
  rg -q '^CONFIG_ESP32S31_SPIFLASH_PSRAM_STACK=y$' "$task_build/.config"
  rg -q '^CONFIG_SCHED_LPWORK=y$' "$task_build/.config"
  rg -q ' [Tt] esp_flash_dispatch$' "$task_build/System.map"
fi
set -o noclobber
sha256sum "$task_build/nuttx.bin" > "$task_receipt"
