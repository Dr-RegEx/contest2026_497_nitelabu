#!/bin/bash
# Separate offline FLAT test build. Never writes or flashes the main demo.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_profile=xts-flat-media
case "$task_profile" in
  xts-flat|xts-flat-adc|xts-flat-rtc|xts-flat-flash|xts-flat-wdt|xts-flat-wdt-romdelay|xts-flat-gpio|xts-flat-bmi160|xts-flat-category-fs|xts-flat-category-kv|xts-flat-bmi160-uorb|xts-flat-pwm|xts-flat-category-fs-flash|xts-flat-category-fs-perf|xts-flat-category-fs-name|xts-flat-category-fs-large|xts-flat-category-fs-sync|xts-flat-category-fs-recovery|xts-flat-category-fs-internal|xts-flat-flash-raw|xts-flat-audio|xts-flat-audio-duplex|xts-flat-audio-continuous|xts-flat-audio-final|xts-flat-audio-mono|xts-flat-media|xts-flat-media-volume|xts-flat-ble|xts-flat-bttool|xts-flat-usb-adb) ;;
  *) echo 'Unsupported FLAT profile' >&2; exit 1 ;;
esac
task_build=$task_root/openvela-dev/out/esp32s31-xts-flat-media1199
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
if [ "$task_profile" = xts-flat-wdt ] || [ "$task_profile" = xts-flat-wdt-romdelay ]; then
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
if [ "$task_profile" = xts-flat-category-fs-flash ] || [ "$task_profile" = xts-flat-category-fs-perf ] || [ "$task_profile" = xts-flat-category-fs-name ] || [ "$task_profile" = xts-flat-category-fs-recovery ] || [ "$task_profile" = xts-flat-category-fs-internal ]; then
  for task_option in ESP32S31_XTS_FLASH FS_LITTLEFS FS_TEST_POWEROFF; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in power_off_test01_main power_off_test02_main power_off_test03_main; do
    rg -q "$task_symbol" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-category-fs-perf ] || [ "$task_profile" = xts-flat-category-fs-name ]; then
  rg -q '^CONFIG_NSH_CMDOPT_DD_STATS=y$' "$task_build/.config"
  rg -q '^CONFIG_TESTING_TESTCASES_PRIORITY=255$' "$task_build/.config"
fi
if [ "$task_profile" = xts-flat-category-fs-large ] || [ "$task_profile" = xts-flat-category-fs-sync ]; then
  for task_option in ESP32S31_XTS_FLASH ESP32S31_XTS_FLASH_LARGE FS_LITTLEFS FS_TEST_STRESS FS_TEST_STABILITY BOARDCTL_RESET ESP32S31_SPIFLASH_PSRAM_STACK; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q '^CONFIG_NSH_LINELEN=128$' "$task_build/.config"
  rg -q '^CONFIG_BOARD_RESET_ON_ASSERT=2$' "$task_build/.config"
  rg -q '^CONFIG_TESTING_TESTCASES_STACKSIZE=32768$' "$task_build/.config"
  if rg -q '^CONFIG_ESP32S31_XTS_MEDIA_VOLUME=y$' "$task_build/.config"; then
    echo 'ERROR: large FS scratch overlaps WAV Flash' >&2
    exit 1
  fi
fi
if [ "$task_profile" = xts-flat-category-fs-internal ]; then
  for task_option in ESP32S31_XTS_FLASH FS_LITTLEFS FS_TEST_STRESS; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  if rg -q '^CONFIG_ESPRESSIF_SPIRAM_USER_HEAP=y$' "$task_build/.config"; then
    echo 'ERROR: internal FS profile unexpectedly enables PSRAM user heap' >&2
    exit 1
  fi
  rg -q '^CONFIG_MM_REGIONS=1$' "$task_build/.config"
  rg -q ' [Tt] vela_fs_stress_loop_create_delete_file_test_main$' "$task_build/System.map"
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
if [ "$task_profile" = xts-flat-media ] || [ "$task_profile" = xts-flat-media-volume ]; then
  task_ffconfig="$task_build/apps/external/ffmpeg/config.h"
  rg -q '^#define CONFIG_HARDCODED_TABLES 1$' "$task_ffconfig"
  rg -q '^#define CONFIG_TX_FLOAT 1$' "$task_ffconfig"
  rg -q '^#define CONFIG_TX_DOUBLE 0$' "$task_ffconfig"
  rg -q '^#define CONFIG_TX_INT32 0$' "$task_ffconfig"
  for task_option in ESP32S31_I2S_DUPLEX ES8311_SHARED_DUPLEX ES8311_PCM_CAPS MEDIA MEDIA_SERVER MEDIA_GRAPH MEDIA_TOOL MEDIA_POLICY LIB_PFW KVDB_DIRECT AUDIOUTILS_ALSA_LIB AUDIOUTILS_ALSA_LIB_DEVICE_HW LIB_FFMPEG LIB_FFMPEG_AUDIO_TABLEGEN EVENT_FD NET_LOCAL NETDEV_LATEINIT; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in mediatool_main mediad_main esp32s31_i2s_duplex_initialize; do
    rg -q " [Tt] ${task_symbol}$" "$task_build/System.map"
  done
  for task_option in ESPRESSIF_WIFI NET_IPv4 NET_IPv6 SMP; do
    if rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"; then
      echo "ERROR: media profile unexpectedly enabled ${task_option}" >&2
      exit 1
    fi
  done
fi
if [ "$task_profile" = xts-flat-media-volume ]; then
  for task_option in ESP32S31_XTS_MEDIA_VOLUME ESPRESSIF_SPIFLASH ESPRESSIF_MTD ESP32S31_SPIFLASH_PSRAM_STACK SCHED_LPWORK FS_LITTLEFS RAMMTD; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q '^CONFIG_SCHED_LPWORKSTACKSIZE=4096$' "$task_build/.config"
  for task_symbol in esp32s31_xts_media_volume_initialize rammtd_initialize_with_config esp_flash_dispatch; do
    rg -q " [Tt] ${task_symbol}$" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-wdt-romdelay ]; then
  rg -q '^CONFIG_ESP32S31_WDT_ROM_DELAY=y$' "$task_build/.config"
  rg -q ' [Tt] esp32s31_wdt_delay_check$' "$task_build/System.map"
fi
if [ "$task_profile" = xts-flat-audio-mono ]; then
  for task_option in ESP32S31_I2S_DUPLEX ES8311_SHARED_DUPLEX ESP32S31_I2S_CAPTURE_16K_MONO; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q ' [Tt] esp32s31_i2s_duplex_initialize$' "$task_build/System.map"
fi
if [ "$task_profile" = xts-flat-audio-duplex ] || [ "$task_profile" = xts-flat-audio-continuous ] || [ "$task_profile" = xts-flat-audio-final ]; then
  for task_option in ESP32S31_AUDIO ESP32S31_I2S ESP32S31_I2S_DUPLEX AUDIO_ES8311 ES8311_SHARED_DUPLEX SYSTEM_NXLOOPER NXLOOPER_INCLUDE_PREFERRED_DEVICE; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in esp32s31_audio_initialize esp32s31_i2s_duplex_initialize nxlooper_main; do
    rg -q " [Tt] ${task_symbol}$" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-audio-continuous ] || [ "$task_profile" = xts-flat-audio-final ]; then
  rg -q '^CONFIG_ESP32S31_I2S_DUPLEX_44K=y$' "$task_build/.config"
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
