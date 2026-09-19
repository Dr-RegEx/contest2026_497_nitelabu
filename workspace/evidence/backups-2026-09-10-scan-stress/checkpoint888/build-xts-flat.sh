#!/bin/bash
# Separate offline FLAT test build. Never writes or flashes the main demo.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_profile=${S31_FLAT_PROFILE:-xts-flat}
case "$task_profile" in
  xts-flat|xts-flat-rtc|xts-flat-flash|xts-flat-wdt|xts-flat-gpio|xts-flat-bmi160) ;;
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
set -o noclobber
sha256sum "$task_build/nuttx.bin" > "$task_receipt"
