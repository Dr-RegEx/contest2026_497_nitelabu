#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint505
test ! -e "$task_cp"
test ! -e "$task_dir/progress492-505.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_cp/apps-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/nuttx-after532a.bundle" \
  refs/heads/codex/esp32s31-port ^532aa75837e0a45507d9c164fec2ba0d154e5543
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/nuttx-after532a.bundle"
git -C "$task_root/openvela-dev/nuttx" ls-files --others --exclude-standard -z > "$task_cp/untracked.list"
tar -czf "$task_cp/nuttx-untracked.tar.gz" -C "$task_root/openvela-dev/nuttx" --null -T "$task_cp/untracked.list"
tar -czf "$task_cp/firmware503.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress491
rg --files logs | rg '(49[2-9]|50[0-5])' > "$task_cp/log-files.txt"
tar -czf progress492-505.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint505 checkpoint505.sh progress503-rmt-errors.md rmt-followup494.md \
  build494-rmt-acquire.sha256 build503-rmt-init.sha256 \
  build-demo.sh flash-demo-pair.sh verify-host-current.sh SHA256SUMS-progress491
sha256sum progress492-505.tar.gz > SHA256SUMS-progress505
sha256sum -c SHA256SUMS-progress505
tar -tzf progress492-505.tar.gz > "$task_cp/archive-index.txt"
