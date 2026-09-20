#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_build=$task_root/openvela-dev/out/esp32s31-make-rmt420/nuttx
set -x
test "$(stat -c %s "$task_dir/demo-before272/before-a.bin")" -eq 5242880
test "$(stat -c %s "$task_dir/demo-before272/before-b.bin")" -eq 5242880
(cd "$task_dir/demo-before272"; sha256sum -c SHA256SUMS)
cmp "$task_dir/demo-before272/before-a.bin" "$task_dir/demo-before272/before-b.bin"
(cd "$task_dir"; sha256sum -c SHA256SUMS-progress433)
sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" | cmp -s - "$task_dir/build432-make-rmt.sha256"
python3 "$task_dir/verify-wifi-artifact.py" "$task_build" --objdump /home/regex/.espressif/tools/riscv32-esp-elf/esp-15.2.0_20251204/riscv32-esp-elf/bin/riscv32-esp-elf-objdump
rg -q '^# CONFIG_ESPRESSIF_WIFI_STA_11AX is not set$' "$task_build/.config"
for task_option in EXAMPLES_S31DEMO EXAMPLES_S31LED ESP_RMT ESPRESSIF_I2C0 SMP BUILD_KERNEL; do
  rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
done
test "$(stat -c %s "$task_build/nuttx.bin")" -le 2088960
test "$(stat -c %s "$task_build/appfs.img")" -le 3145728
for task_app in init sh s31demo s31led i2c buttons wapi ping renew; do
  test "$(sha256sum "$task_build/appfs-root/$task_app" | cut -d ' ' -f 1)" = "$(head -n 1 "$task_build/appfs-root/$task_app.sha256")"
done
/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool \
  --chip esp32s31 \
  --port /dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0 \
  --baud 460800 write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m \
  0x2000 "$task_build/nuttx.bin" 0x200000 "$task_build/appfs.img"
