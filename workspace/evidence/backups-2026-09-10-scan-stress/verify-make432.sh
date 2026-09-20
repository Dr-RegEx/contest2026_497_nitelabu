#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_build=$task_root/openvela-dev/out/esp32s31-make-rmt420/nuttx
task_tools=/home/regex/.espressif/tools/riscv32-esp-elf/esp-15.2.0_20251204/riscv32-esp-elf/bin
cd "$task_build"
set -x
python3 "$task_root/backups/2026-09-10-scan-stress/verify-wifi-artifact.py" . --objdump "$task_tools/riscv32-esp-elf-objdump"
test "$(stat -c %s nuttx.bin)" -le 2088960
test "$(stat -c %s appfs.img)" -le 3145728
for task_app in init sh s31demo s31led i2c buttons wapi ping renew; do
  test -f "appfs-root/$task_app"
  test "$(stat -c %s "appfs-root/$task_app")" -le 524288
  test "$(sha256sum "appfs-root/$task_app" | cut -d ' ' -f 1)" = "$(head -n 1 "appfs-root/$task_app.sha256")"
done
for task_app in s31led s31demo; do
  python3 tools/test_esp32s31_elf_metadata.py --nm "$task_tools/riscv32-esp-elf-nm" --elf "appfs-root/$task_app" --reference "../apps/bin/$task_app" --check-make
done
/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool --chip esp32s31 image-info nuttx.bin
test ! -e "$task_root/backups/2026-09-10-scan-stress/build432-make-rmt.sha256"
sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" > "$task_root/backups/2026-09-10-scan-stress/build432-make-rmt.sha256"
echo MAKE_RMT_ARTIFACTS=PASS
