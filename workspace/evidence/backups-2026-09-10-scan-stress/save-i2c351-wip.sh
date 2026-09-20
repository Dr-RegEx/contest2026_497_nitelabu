#!/bin/bash
# Snapshot an unfinished diagnostic group; this is not an acceptance marker.
set -euo pipefail
set -o noclobber
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_nuttx=$task_root/openvela-dev/nuttx
task_apps=$task_root/openvela-dev/apps
task_build=$task_root/openvela-dev/out/esp32s31-cmake-demo
cd "$task_dir"
for task_name in checkpoint351-wip progress326-351-i2c-wip.tar.gz \
  firmware350-i2c-wip.tar.gz esp32s31-port-d468-to-9e8c.bundle \
  esp32s31-apps-59b0-to-e498.bundle SHA256SUMS-i2c351-wip; do
  test ! -e "$task_name"
done
sha256sum -c build350-i2c-spurious.sha256
mkdir checkpoint351-wip
git -C "$task_nuttx" bundle create "$task_dir/esp32s31-port-d468-to-9e8c.bundle" \
  d468c494ff8..codex/esp32s31-port
git -C "$task_apps" bundle create "$task_dir/esp32s31-apps-59b0-to-e498.bundle" \
  59b05cd92..codex/esp32s31-nettest
git -C "$task_nuttx" bundle verify "$task_dir/esp32s31-port-d468-to-9e8c.bundle"
git -C "$task_apps" bundle verify "$task_dir/esp32s31-apps-59b0-to-e498.bundle"
git -C "$task_nuttx" diff --binary > checkpoint351-wip/nuttx-working.patch
git -C "$task_nuttx" status --short > checkpoint351-wip/nuttx-status.txt
git -C "$task_apps" status --short > checkpoint351-wip/apps-status.txt
git -C "$task_nuttx" rev-parse HEAD > checkpoint351-wip/nuttx-head.txt
git -C "$task_apps" rev-parse HEAD > checkpoint351-wip/apps-head.txt
tar -czf checkpoint351-wip/new-i2c-files.tar.gz -C "$task_nuttx" \
  tools/test_esp_i2c_commands.py tools/test_esp32s31_i2c_reset.py \
  tools/test_esp_i2c_spurious_irq.py \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-i2c
tar -czf firmware350-i2c-wip.tar.gz -C "$task_build" \
  nuttx nuttx.bin nuttx.map .config .config.prev include/nuttx/config.h \
  appfs.img bin bin_debug
mapfile -t task_logs < <(rg --files logs | \
  rg '/[a-z-]*(32[6-9]|33[0-9]|34[0-9]|350|351)[^0-9]' | \
  rg -v 'checkpoint351-wip.log$')
mapfile -t task_receipts < <(rg --files -g '*.sha256' | \
  rg '^build(32[6-9]|33[0-9]|34[0-9]|350)[^0-9]')
tar -czf progress326-351-i2c-wip.tar.gz README.md checkpoint351-wip \
  firmware350-i2c-wip.tar.gz build-demo.sh flash-demo-pair.sh \
  i2c333-codec-id.py i2c338-codec-stress.py read-schematic331.py \
  schematic331-audio.png save-i2c351-wip.sh \
  esp32s31-port-d468-to-9e8c.bundle esp32s31-apps-59b0-to-e498.bundle \
  "${task_logs[@]}" "${task_receipts[@]}"
sha256sum progress326-351-i2c-wip.tar.gz firmware350-i2c-wip.tar.gz \
  esp32s31-port-d468-to-9e8c.bundle esp32s31-apps-59b0-to-e498.bundle \
  > SHA256SUMS-i2c351-wip
sha256sum -c SHA256SUMS-i2c351-wip
printf 'I2C_CHECKPOINT351_WIP=PASS backup only; concurrent I2C acceptance pending\n'
