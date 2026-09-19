#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint586
test ! -e "$task_cp"
test ! -e "$task_dir/progress574-586.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_cp/apps-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/rmt-tx-lifecycle.bundle" b40bb54c514..HEAD
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/rmt-tx-lifecycle.bundle"
tar -czf "$task_cp/firmware582.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress573
rg --files logs | rg '(57[4-9]|58[0-6])' > "$task_cp/log-files.txt"
tar -czf progress574-586.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint586 checkpoint586.sh progress574-rmt-tx-validation.md \
  build582-rmt-tx-lifecycle.sha256 build-demo.sh flash-demo-pair.sh SHA256SUMS-progress573
sha256sum progress574-586.tar.gz > SHA256SUMS-progress586
sha256sum -c SHA256SUMS-progress586
tar -tzf progress574-586.tar.gz > "$task_cp/archive-index.txt"
