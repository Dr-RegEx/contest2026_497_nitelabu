#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint653
test ! -e "$task_cp"
test ! -e "$task_dir/progress646-653.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/isr-woken.bundle" 8fb1462ee68..HEAD
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/isr-woken.bundle"
cd "$task_dir"
sha256sum -c build648-isr-woken.sha256
sha256sum -c SHA256SUMS-progress640
sha256sum -c SHA256SUMS-progress645
tar -czf "$task_cp/firmware648.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" nuttx nuttx.bin appfs.img .config
rg --files logs | rg '/(host64[6-9]-|host650-|build648-|flash650-|demo651-|http652-)' > "$task_cp/log-files.txt"
tar -czf progress646-653.tar.gz -T "$task_cp/log-files.txt" checkpoint653 \
  checkpoint653.sh progress646-isr-woken.md isr650-index.py isr650-index.patch \
  build648-isr-woken.sha256 build-demo.sh flash-demo-pair.sh \
  SHA256SUMS-progress640 SHA256SUMS-progress645
sha256sum progress646-653.tar.gz > SHA256SUMS-progress653
sha256sum -c SHA256SUMS-progress653
tar -tzf progress646-653.tar.gz > "$task_cp/archive-index.txt"
