#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint573
test ! -e "$task_cp"
test ! -e "$task_dir/progress569-573.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-wip.patch"
git -C "$task_root/openvela-dev/nuttx" ls-files --others --exclude-standard -z > "$task_cp/untracked.list"
tar -czf "$task_cp/nuttx-untracked.tar.gz" -C "$task_root/openvela-dev/nuttx" --null -T "$task_cp/untracked.list"
tar -czf "$task_cp/firmware572.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress568
rg --files logs | rg '(569|57[0-3])' > "$task_cp/log-files.txt"
tar -czf progress569-573.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint573 checkpoint573.sh progress569-rmt-tx-wip.md \
  build572-rmt-tx-lifecycle.sha256 build-demo.sh flash-demo-pair.sh SHA256SUMS-progress568
sha256sum progress569-573.tar.gz > SHA256SUMS-progress573
sha256sum -c SHA256SUMS-progress573
tar -tzf progress569-573.tar.gz > "$task_cp/archive-index.txt"
