#!/bin/bash
set -euo pipefail
set -x
receipt=${1:?Pass the unique successful-build SHA256 receipt}
flash_mode=${2:-kernel}
case "$flash_mode" in
  kernel|appfs) ;;
  *) echo "Expected kernel or appfs" >&2; exit 2 ;;
esac
firmware_dir=/home/regex/work/esp32s31-openvela/openvela-dev/out/esp32s31-cmake-production6
sha256sum "$firmware_dir/nuttx.bin" "$firmware_dir/appfs.img" | cmp -s - "$receipt"
flash_args=(0x2000 "$firmware_dir/nuttx.bin")
if [ "$flash_mode" = appfs ]; then
  flash_args+=(0x200000 "$firmware_dir/appfs.img")
fi
/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool \
  --chip esp32s31 --port /dev/ttyUSB0 --baud 460800 \
  write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m \
  "${flash_args[@]}"
