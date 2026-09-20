#!/bin/bash
# Explicit HE diagnostic gate, same two authorized regions as the basic demo.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_build=$task_root/openvela-dev/out/esp32s31-cmake-demo
task_receipt=${1:?Pass the successful HE diagnostic build receipt}
test "$(stat -c %s "$task_dir/demo-before272/before-a.bin")" -eq 5242880
test "$(stat -c %s "$task_dir/demo-before272/before-b.bin")" -eq 5242880
(cd "$task_dir/demo-before272"; sha256sum -c SHA256SUMS)
cmp "$task_dir/demo-before272/before-a.bin" "$task_dir/demo-before272/before-b.bin"
(cd "$task_dir"; sha256sum -c SHA256SUMS-progress470)
sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" | cmp -s - "$task_receipt"
python3 "$task_dir/verify-wifi-artifact.py" "$task_build" --objdump \
  /home/regex/.espressif/tools/riscv32-esp-elf/esp-15.2.0_20251204/riscv32-esp-elf/bin/riscv32-esp-elf-objdump
for task_option in ESPRESSIF_WIFI_STA_11AX NET_ARP_IPIN NET_STATISTICS EXAMPLES_S31DEMO EXAMPLES_S31LED; do
  rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
done
test -f "$task_build/appfs-root/s31demo"
test -f "$task_build/appfs-root/s31led"
test "$(stat -c %s "$task_build/nuttx.bin")" -le 2088960
test "$(stat -c %s "$task_build/appfs.img")" -le 3145728
set -x
/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool \
  --chip esp32s31 \
  --port /dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0 \
  --baud 460800 write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m \
  0x2000 "$task_build/nuttx.bin" 0x200000 "$task_build/appfs.img"
