#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint564
test ! -e "$task_cp"
test ! -e "$task_dir/progress558-564.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_cp/apps-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/rmt-install.bundle" 39be74d0093..HEAD
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/rmt-install.bundle"
tar -czf "$task_cp/firmware559.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress557
rg --files logs | rg '(55[89]|56[0-4])' > "$task_cp/log-files.txt"
tar -czf progress558-564.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint564 checkpoint564.sh progress558-rmt-install.md \
  build559-rmt-install.sha256 build-demo.sh flash-demo-pair.sh SHA256SUMS-progress557
sha256sum progress558-564.tar.gz > SHA256SUMS-progress564
sha256sum -c SHA256SUMS-progress564
tar -tzf progress558-564.tar.gz > "$task_cp/archive-index.txt"
