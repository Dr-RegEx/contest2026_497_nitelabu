#!/bin/bash
# Temporary FLAT test image: write only the existing kernel slot at0x2000.
# Preserve the AppFS and all persistent partitions. Never use during a test.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_profile=${S31_FLAT_PROFILE:-xts-flat}
case "$task_profile" in
  xts-flat|xts-flat-rtc|xts-flat-flash|xts-flat-wdt|xts-flat-gpio|xts-flat-bmi160) ;;
  *) echo 'Unsupported FLAT profile' >&2; exit 1 ;;
esac
task_build=$task_root/openvela-dev/out/esp32s31-$task_profile
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
set -x
/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool \
  --chip esp32s31 \
  --port /dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0 \
  --baud 460800 write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m \
  0x2000 "$task_build/nuttx.bin"
