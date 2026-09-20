#!/bin/bash
# The new demo adds an AppFS executable. Back up all possibly affected sectors.
set -euo pipefail
task_dir=/home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/demo-before272
test ! -e "$task_dir"
mkdir "$task_dir"
task_esptool=/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool
task_port=/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0
set -x
"$task_esptool" --chip esp32s31 --port "$task_port" --baud 460800 \
  read-flash 0x0 0x500000 "$task_dir/before-a.bin"
"$task_esptool" --chip esp32s31 --port "$task_port" --baud 460800 \
  read-flash 0x0 0x500000 "$task_dir/before-b.bin"
test "$(stat -c %s "$task_dir/before-a.bin")" -eq 5242880
cmp "$task_dir/before-a.bin" "$task_dir/before-b.bin"
cd "$task_dir"
sha256sum before-a.bin before-b.bin > SHA256SUMS
sha256sum -c SHA256SUMS
echo DEMO_BEFORE_BACKUP=PASS
