#!/bin/bash
# Restore only the authorized kernel and the exact saved AppFS sectors.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_record=$task_root/backups/2026-09-10-scan-stress
task_kernel=$task_root/openvela-dev/out/esp32s31-cmake-production6/nuttx.bin
task_appfs=$task_record/appfs-before198.bin
task_readback=${1:?Pass a new readback filename}
test ! -e "$task_readback"
test "$(sha256sum "$task_kernel" | cut -d' ' -f1)" = 9f77317b39b36dcc871805f7d8e56aa5d2224f4340ec3845b5c8fa5ec185eda9
test "$(sha256sum "$task_appfs" | cut -d' ' -f1)" = 0d9ac9f69bc7687814f7a2374744236b89b9539e334a4017b8e1e90c8733cd5c
test "$(stat -c %s "$task_appfs")" = 544768
task_esptool=/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool
task_port=/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0
set -x
"$task_esptool" --chip esp32s31 --port "$task_port" --baud 460800 \
  write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m \
  0x2000 "$task_kernel" 0x200000 "$task_appfs"
"$task_esptool" --chip esp32s31 --port "$task_port" --baud 460800 \
  read-flash 0x200000 0x85000 "$task_readback"
cmp "$task_appfs" "$task_readback"
sha256sum "$task_appfs" "$task_readback"
echo PRODUCTION180_PAIR_RESTORE=PASS
