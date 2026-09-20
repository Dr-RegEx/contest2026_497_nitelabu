#!/bin/bash
set -euo pipefail
set -o noclobber
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_nuttx=$task_root/openvela-dev/nuttx
task_build=$task_root/openvela-dev/out/esp32s31-cmake-demo
cd "$task_dir"
for task_name in checkpoint397 firmware388-i2c-clean.tar.gz \
  esp32s31-port-9e8c-to-d431.bundle progress372-397-i2c.tar.gz \
  SHA256SUMS-progress397; do
  test ! -e "$task_name"
done
sha256sum -c build388-i2c-clean.sha256
sha256sum -c SHA256SUMS-firmware377
mkdir checkpoint397
git -C "$task_nuttx" bundle create "$task_dir/esp32s31-port-9e8c-to-d431.bundle" \
  9e8cae06cf5..codex/esp32s31-port
git -C "$task_nuttx" bundle verify "$task_dir/esp32s31-port-9e8c-to-d431.bundle"
git -C "$task_nuttx" diff --binary > checkpoint397/nuttx-working.patch
git -C "$task_nuttx" status --short > checkpoint397/nuttx-status.txt
git -C "$task_nuttx" rev-parse HEAD > checkpoint397/nuttx-head.txt
git -C "$task_root/openvela-dev/apps" status --short > checkpoint397/apps-status.txt
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > checkpoint397/apps-head.txt
tar -czf checkpoint397/older-diagnostic-profiles.tar.gz -C "$task_nuttx" \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-cpu0 \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-regression \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/wifi-flat
tar -czf firmware388-i2c-clean.tar.gz -C "$task_build" \
  nuttx nuttx.bin nuttx.map .config .config.prev include/nuttx/config.h \
  appfs.img bin bin_debug
mapfile -t task_logs < <(rg --files logs | \
  rg '/[a-z-]*(37[2-9]|38[0-9]|39[0-6])[^0-9]')
mapfile -t task_receipts < <(rg --files -g '*.sha256' | \
  rg '^build(37[2-9]|38[0-9]|39[0-6])[^0-9]')
tar -czf progress372-397-i2c.tar.gz checkpoint397 \
  firmware377-i2c-buffer.tar.gz firmware377-source.patch SHA256SUMS-firmware377 \
  firmware388-i2c-clean.tar.gz esp32s31-port-9e8c-to-d431.bundle \
  build-demo.sh flash-demo-pair.sh serial-idle395.py \
  progress366-i2c-investigation.md progress397-i2c-accepted.md save-progress397.sh \
  "${task_logs[@]}" "${task_receipts[@]}"
sha256sum progress372-397-i2c.tar.gz firmware388-i2c-clean.tar.gz \
  esp32s31-port-9e8c-to-d431.bundle > SHA256SUMS-progress397
sha256sum -c SHA256SUMS-progress397
printf 'I2C_CHECKPOINT397=PASS\n'
