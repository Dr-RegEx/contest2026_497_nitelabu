#!/bin/bash
# User-authorized IDF comparison: read-only backup before any IDF writes.
set -euo pipefail
umask 077
backup_dir=/home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/idf170-flash-backup
identity=/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0
test "$(readlink -f "$identity")" = /dev/ttyUSB0
mkdir "$backup_dir"
exec > >(tee "$backup_dir/backup.log") 2>&1
esptool=/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool
set -x
"$esptool" --chip esp32s31 --port "$identity" --baud 460800 \
  read-flash 0x0 0x500000 "$backup_dir/before-a.bin"
"$esptool" --chip esp32s31 --port "$identity" --baud 460800 \
  read-flash 0x0 0x500000 "$backup_dir/before-b.bin"
test "$(stat -c %s "$backup_dir/before-a.bin")" = 5242880
test "$(stat -c %s "$backup_dir/before-b.bin")" = 5242880
cmp "$backup_dir/before-a.bin" "$backup_dir/before-b.bin"
sha256sum "$backup_dir/before-a.bin" "$backup_dir/before-b.bin" > "$backup_dir/SHA256SUMS"
sha256sum --check "$backup_dir/SHA256SUMS"
echo BACKUP_DOUBLE_READ=PASS
