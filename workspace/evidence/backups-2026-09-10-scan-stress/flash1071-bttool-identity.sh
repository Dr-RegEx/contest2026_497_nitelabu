#!/bin/bash
# Temporary FLAT test image: write only the existing kernel slot at0x2000.
# Preserve the AppFS and all persistent partitions. Never use during a test.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_profile=${S31_FLAT_PROFILE:-xts-flat-bttool}
if [[ "$task_profile" != xts-flat-bttool ]]; then
  echo "ERROR: 1071 helper only accepts xts-flat-bttool" >&2
  exit 1
fi
case "$task_profile" in
  xts-flat|xts-flat-adc|xts-flat-rtc|xts-flat-flash|xts-flat-wdt|xts-flat-wdt-romdelay|xts-flat-gpio|xts-flat-bmi160|xts-flat-category-fs|xts-flat-category-kv|xts-flat-bmi160-uorb|xts-flat-pwm|xts-flat-category-fs-flash|xts-flat-category-fs-perf|xts-flat-category-fs-name|xts-flat-category-fs-large|xts-flat-category-fs-recovery|xts-flat-category-fs-internal|xts-flat-flash-raw|xts-flat-audio|xts-flat-audio-duplex|xts-flat-media|xts-flat-media-volume|xts-flat-bttool|xts-flat-usb-adb) ;;
  *) echo 'Unsupported FLAT profile' >&2; exit 1 ;;
esac
task_build=$task_root/openvela-dev/out/esp32s31-xts-flat-bttool1071
task_receipt=${1:?Pass the successful FLAT kernel SHA receipt}
command -v fuser >/dev/null
if fuser /dev/ttyUSB0; then
  echo 'ERROR: UART busy; refusing reset/flash' >&2
  exit 1
fi
test "$(stat -c %s "$task_dir/demo-before272/before-a.bin")" -eq 5242880
test "$(stat -c %s "$task_dir/demo-before272/before-b.bin")" -eq 5242880
(cd "$task_dir/demo-before272"; sha256sum -c SHA256SUMS)
cmp "$task_dir/demo-before272/before-a.bin" "$task_dir/demo-before272/before-b.bin"
sha256sum "$task_build/nuttx.bin" | cmp -s - "$task_receipt"
for task_option in ARCH_CHIP_ESP32S31 ESPRESSIF_ESP32S31 BUILD_FLAT ESPRESSIF_SIMPLE_BOOT TESTING_MM TESTING_DRIVER_TEST ESPRESSIF_SPIRAM_USE_8LINE_MODE; do
  rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
done
if rg -q '^CONFIG_(BUILD_KERNEL|BUILD_PROTECTED|ESPRESSIF_WIFI|SMP)=y$' "$task_build/.config"; then
  echo 'ERROR: unexpected kernel/radio/SMP configuration' >&2
  exit 1
fi
test "$(stat -c %s "$task_build/nuttx.bin")" -le 2088960
rg -q 'mm_main' "$task_build/System.map"
rg -q 'cmocka_driver_block_main' "$task_build/System.map"
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
if [ "$task_profile" = xts-flat-audio-duplex ]; then
  for task_option in ESP32S31_AUDIO ESP32S31_I2S ESP32S31_I2S_DUPLEX AUDIO_ES8311 ES8311_SHARED_DUPLEX SYSTEM_NXLOOPER NXLOOPER_INCLUDE_PREFERRED_DEVICE; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_symbol in esp32s31_audio_initialize esp32s31_i2s_duplex_initialize nxlooper_main; do
    rg -q " [Tt] ${task_symbol}$" "$task_build/System.map"
  done
fi
if [ "$task_profile" = xts-flat-bttool ]; then
  for task_option in ESP32S31_BLE BLUETOOTH_TOOLS BLUETOOTH_STACK_LE_ZBLUE; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  if rg -q '^CONFIG_ESPRESSIF_SPIRAM_USER_HEAP=y$' "$task_build/.config"; then
    echo 'ERROR: BLE requires internal SRAM for controller allocations' >&2
    exit 1
  fi
  rg -q 'bttool_main' "$task_build/System.map"
fi
if [ "$task_profile" = xts-flat-category-fs-large ]; then
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
if [ "$task_profile" = xts-flat-adc ]; then
  for task_option in ESP32S31_XTS_ADC ADC EXAMPLES_ADC EXAMPLES_ADC_SWTRIG; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q ' [Tt] esp32s31_adc_setup$' "$task_build/System.map"
  rg -q ' [Tt] adc_main$' "$task_build/System.map"
fi
if [ "$task_profile" = xts-flat-usb-adb ]; then
  for task_option in ESP32S31_USBDEV USBDEV USBDEV_FS USBADB SYSTEM_ADBD ADBD_USB_SERVER ADBD_SHELL_SERVICE FS_BINFS; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  if rg -q '^CONFIG_(ESPRESSIF_SPIRAM_USER_HEAP|USBDEV_DMA|USBHOST|ADBD_NET_INIT)=y$' "$task_build/.config"; then
    echo 'ERROR: unexpected USB memory/DMA/host/network configuration' >&2
    exit 1
  fi
  rg -q ' [Tt] esp32s31_usbinitialize$' "$task_build/System.map"
  rg -q ' [Tt] adbd_main$' "$task_build/System.map"
fi
if rg -q '^CONFIG_ESP32S31_XTS_FLASH=y$' "$task_build/.config" &&
   rg -q '^CONFIG_ESPRESSIF_SPIRAM_USER_HEAP=y$' "$task_build/.config"; then
  rg -q '^CONFIG_ESP32S31_SPIFLASH_PSRAM_STACK=y$' "$task_build/.config"
  rg -q '^CONFIG_SCHED_LPWORK=y$' "$task_build/.config"
  rg -q ' [Tt] esp_flash_dispatch$' "$task_build/System.map"
fi
set -x
/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool \
  --chip esp32s31 \
  --port /dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0 \
  --baud 460800 write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m \
  0x2000 "$task_build/nuttx.bin"
