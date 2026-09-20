#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint594
test ! -e "$task_cp"
test ! -e "$task_dir/progress587-594.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/arp-callback.bundle" 92e89d75e91..HEAD
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/arp-callback.bundle"
tar -czf "$task_cp/firmware591.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress586
rg --files logs | rg '(58[7-9]|59[0-4])' > "$task_cp/log-files.txt"
tar -czf progress587-594.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint594 checkpoint594.sh progress587-arp-active.md \
  build591-arp-callback.sha256 build-demo.sh flash-demo-pair.sh \
  network-probe.py network-repeat.py flash-clean551.sh SHA256SUMS-progress586
sha256sum progress587-594.tar.gz > SHA256SUMS-progress594
sha256sum -c SHA256SUMS-progress594
tar -tzf progress587-594.tar.gz > "$task_cp/archive-index.txt"
