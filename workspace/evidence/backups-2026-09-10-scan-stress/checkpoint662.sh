#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint662
test ! -e "$task_cp"
test ! -e "$task_dir/progress654-662.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/queue-zero.bundle" 3eafe33ac56..HEAD
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/queue-zero.bundle"
cd "$task_dir"
sha256sum -c build657-queue-zero.sha256
sha256sum -c SHA256SUMS-progress653
tar -czf "$task_cp/firmware657.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" nuttx nuttx.bin appfs.img .config
rg --files logs | rg '/(host65[4-8][b]?-|build657-|flash659-|demo660-|http661-)' > "$task_cp/log-files.txt"
tar -czf progress654-662.tar.gz -T "$task_cp/log-files.txt" checkpoint662 \
  checkpoint662.sh progress654-queue-zero.md queue658-index.py queue658-index.patch \
  build657-queue-zero.sha256 build-demo.sh flash-demo-pair.sh SHA256SUMS-progress653
sha256sum progress654-662.tar.gz > SHA256SUMS-progress662
sha256sum -c SHA256SUMS-progress662
tar -tzf progress654-662.tar.gz > "$task_cp/archive-index.txt"
