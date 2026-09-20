#!/bin/bash
# Only the demo's ABI-paired kernel/AppFS, after a verified full-range backup.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_build=$task_root/openvela-dev/out/esp32s31-cmake-demo
if [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-netapps-competition-offline ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-competition-offline
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-netapps-competition ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-competition
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-netapps-iperf ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-netapps-iperf
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-netapps-ssh ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-netapps-ssh
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-netapps ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-netapps
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-xts-standby ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-standby
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-xts-ecc-offline-log ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-ecc-offline-log
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-xts-ecc-offline ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-ecc-offline
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-xts-ecc ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-ecc
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-xts-ecdsa ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-ecdsa
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-xts-hmac ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-hmac
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-xts-sha ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-sha
elif [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-xts-aes-modes ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-aes-modes
fi
task_build=$task_root/openvela-dev/out/esp32s31-network1534
task_receipt=${1:?Pass the successful 1328 paired build receipt}
if fuser /dev/ttyUSB0; then
  echo 'ERROR: UART busy; refusing reset/flash' >&2
  exit 1
fi
test "$(stat -c %s "$task_dir/demo-before272/before-a.bin")" -eq 5242880
test "$(stat -c %s "$task_dir/demo-before272/before-b.bin")" -eq 5242880
(cd "$task_dir/demo-before272"; sha256sum -c SHA256SUMS)
cmp "$task_dir/demo-before272/before-a.bin" "$task_dir/demo-before272/before-b.bin"
sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" | cmp -s - "$task_receipt"
python3 "$task_dir/verify-wifi-artifact.py" "$task_build" --objdump \
  /home/regex/.espressif/tools/riscv32-esp-elf/esp-15.2.0_20251204/riscv32-esp-elf/bin/riscv32-esp-elf-objdump
rg -q '^# CONFIG_ESPRESSIF_WIFI_STA_11AX is not set$' "$task_build/.config"
rg -q '^CONFIG_EXAMPLES_S31DEMO=y$' "$task_build/.config"
test -f "$task_build/appfs-root/s31demo"
test "$(stat -c %s "$task_build/nuttx.bin")" -le 2088960
test "$(stat -c %s "$task_build/appfs.img")" -le 3145728
if [[ "${S31_DEMO_PROFILE:-demo}" = demo-rmt-xts-ecc-offline* ]]; then
  rg -q '^CONFIG_NETINIT_NETLOCAL=y$' "$task_build/.config"
  ! rg -q '^CONFIG_NETINIT_DHCPC=y$' "$task_build/.config"
  for task_option in CRYPTO_CRYPTODEV ESP32S31_CRYPTO_AES_CBC ESP32S31_CRYPTO_AES_MODES ESP32S31_CRYPTO_SHA ESP32S31_CRYPTO_HMAC ESP32S31_CRYPTO_ECDSA_VERIFY ESP32S31_CRYPTO_ECC_POINT_MULT; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_app in cmocka_des3cbc cmocka_aescbc cmocka_aesctr cmocka_aesxts cmocka_hmac cmocka_hash cmocka_crc32 cmocka_ecdsa; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
set -x
/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool \
  --chip esp32s31 \
  --port /dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0 \
  --baud 460800 write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m \
  0x2000 "$task_build/nuttx.bin" 0x200000 "$task_build/appfs.img"
