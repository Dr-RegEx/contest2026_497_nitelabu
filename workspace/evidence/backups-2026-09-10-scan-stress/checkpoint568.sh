#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint568
test ! -e "$task_cp"
test ! -e "$task_dir/progress565-568.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/rmt-rx-lock.bundle" 6186228f55c..HEAD
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/rmt-rx-lock.bundle"
tar -czf "$task_cp/firmware567.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress564
rg --files logs | rg '56[5-8]' > "$task_cp/log-files.txt"
tar -czf progress565-568.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint568 checkpoint568.sh progress565-rmt-lifetime.md \
  probe565-rmt-completion.py build567-rmt-rx-lock.sha256 \
  build-demo.sh flash-demo-pair.sh SHA256SUMS-progress564
sha256sum progress565-568.tar.gz > SHA256SUMS-progress568
sha256sum -c SHA256SUMS-progress568
tar -tzf progress565-568.tar.gz > "$task_cp/archive-index.txt"
