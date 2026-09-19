#!/bin/bash
# Only the isolated regression kernel and its matching read-only AppFS.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_build=$task_root/openvela-dev/out/esp32s31-cmake-wifi-regression209
task_receipt=${1:?Pass the successful build receipt}
sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" | cmp -s - "$task_receipt"
python3 "$task_root/backups/2026-09-10-scan-stress/verify-wifi-artifact.py" \
  "$task_build" --objdump \
  /home/regex/.espressif/tools/riscv32-esp-elf/esp-15.2.0_20251204/riscv32-esp-elf/bin/riscv32-esp-elf-objdump
# The newer demo checkpoint backs up the entire affected read-only range.
task_backup=$task_root/backups/2026-09-10-scan-stress/demo-before272
test "$(stat -c %s "$task_backup/before-a.bin")" -eq 5242880
test "$(stat -c %s "$task_backup/before-b.bin")" -eq 5242880
(cd "$task_backup"; sha256sum -c SHA256SUMS)
cmp "$task_backup/before-a.bin" "$task_backup/before-b.bin"
# Never extend into the writable partition at 0x500000.
test "$(stat -c %s "$task_build/nuttx.bin")" -le 2088960
test "$(stat -c %s "$task_build/appfs.img")" -le 3145728
set -x
/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool \
  --chip esp32s31 \
  --port /dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0 \
  --baud 460800 write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m \
  0x2000 "$task_build/nuttx.bin" 0x200000 "$task_build/appfs.img"
