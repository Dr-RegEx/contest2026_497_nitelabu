#!/bin/bash
set -euo pipefail

task_root=/home/regex/work/esp32s31-openvela
task_build=$task_root/openvela-dev/out/esp32s31-ble-controller-info1747
task_receipt=$task_root/backups/2026-09-10-scan-stress/build1747-ble-controller-info.sha256
task_port=/dev/ttyUSB0
test -f "$task_receipt"
test "$(stat -c %s "$task_build/nuttx.bin")" -le 2088960
test "$(stat -c %s "$task_build/appfs.img")" -le 3145728
sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" | cmp -s - "$task_receipt"
test -e "$task_port"
"/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool" \
  --chip esp32s31 --port /dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0 --baud 460800 write-flash \
  --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x2000 "$task_build/nuttx.bin" 0x200000 "$task_build/appfs.img"
