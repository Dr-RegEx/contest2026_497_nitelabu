#!/bin/bash
set -euo pipefail

task_root=/home/regex/work/esp32s31-openvela
task_build=$task_root/openvela-dev/out/esp32s31-ble-multi1743
task_receipt=${1:?Pass the paired multi-connection receipt}

if fuser /dev/ttyUSB0; then
  echo 'ERROR: UART busy; refusing reset/flash' >&2
  exit 1
fi

sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" | cmp -s - "$task_receipt"
rg -q '^CONFIG_BT_MAX_CONN=2$' "$task_build/.config"
rg -q '^CONFIG_BT_EXT_ADV_MAX_ADV_SET=2$' "$task_build/.config"
test -f "$task_build/appfs-root/bttool"

/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool \
  --chip esp32s31 \
  --port /dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0 \
  --baud 460800 write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m \
  0x2000 "$task_build/nuttx.bin" 0x200000 "$task_build/appfs.img"
