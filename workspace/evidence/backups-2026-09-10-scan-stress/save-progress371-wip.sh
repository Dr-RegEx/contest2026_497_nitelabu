#!/bin/bash
# Preserve the unfinished I2C work without labeling diagnostics as accepted.
set -euo pipefail
set -o noclobber
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_nuttx=$task_root/openvela-dev/nuttx
task_build=$task_root/openvela-dev/out/esp32s31-cmake-demo
cd "$task_dir"
for task_name in checkpoint371-wip firmware366-i2c-wip.tar.gz \
  progress352-371-i2c-wip.tar.gz SHA256SUMS-i2c371-wip; do
  test ! -e "$task_name"
done
sha256sum -c build366-i2c-events.sha256
mkdir checkpoint371-wip
git -C "$task_nuttx" diff --binary > checkpoint371-wip/nuttx-working.patch
git -C "$task_nuttx" status --short > checkpoint371-wip/nuttx-status.txt
git -C "$task_nuttx" rev-parse HEAD > checkpoint371-wip/nuttx-head.txt
git -C "$task_root/openvela-dev/apps" status --short > checkpoint371-wip/apps-status.txt
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > checkpoint371-wip/apps-head.txt
tar -czf checkpoint371-wip/new-i2c-files.tar.gz -C "$task_nuttx" \
  tools/test_esp_i2c_commands.py tools/test_esp32s31_i2c_reset.py \
  tools/test_esp_i2c_spurious_irq.py tools/test_esp_i2c_transfer_lock.py \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-i2c \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-i2c-cpu1-init
tar -czf firmware366-i2c-wip.tar.gz -C "$task_build" \
  nuttx nuttx.bin nuttx.map .config .config.prev include/nuttx/config.h \
  appfs.img bin bin_debug
mapfile -t task_logs < <(rg --files logs | \
  rg '/[a-z-]*(35[2-9]|36[0-9]|370)[^0-9]')
mapfile -t task_receipts < <(rg --files -g '*.sha256' | \
  rg '^build(35[2-9]|36[0-9]|370)[^0-9]')
tar -czf progress352-371-i2c-wip.tar.gz checkpoint371-wip \
  firmware366-i2c-wip.tar.gz build-demo.sh flash-demo-pair.sh \
  i2c338-codec-stress.py progress366-i2c-investigation.md \
  save-progress371-wip.sh "${task_logs[@]}" "${task_receipts[@]}"
sha256sum progress352-371-i2c-wip.tar.gz firmware366-i2c-wip.tar.gz \
  > SHA256SUMS-i2c371-wip
sha256sum -c SHA256SUMS-i2c371-wip
printf 'I2C_CHECKPOINT371_WIP=PASS backup only; acceptance pending\n'
