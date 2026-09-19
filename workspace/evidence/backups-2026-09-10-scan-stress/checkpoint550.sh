#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint550
test ! -e "$task_cp"
test ! -e "$task_dir/progress543-550.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_cp/apps-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" ls-files --others --exclude-standard -z > "$task_cp/untracked.list"
tar -czf "$task_cp/nuttx-untracked.tar.gz" -C "$task_root/openvela-dev/nuttx" --null -T "$task_cp/untracked.list"
tar -czf "$task_cp/firmware547.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress542
rg --files logs | rg '(54[3-9]|550)' > "$task_cp/log-files.txt"
tar -czf progress543-550.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint550 checkpoint550.sh progress546-arp-request.md \
  firmware544-arp-request.tar.gz diagnostics544-arp-request.patch \
  build544-arp-request.sha256 build547-restore-demo.sha256 \
  build-demo.sh flash-demo-pair.sh network-probe.py network-repeat.py \
  tcp-nettest-client.py SHA256SUMS-progress542
sha256sum progress543-550.tar.gz > SHA256SUMS-progress550
sha256sum -c SHA256SUMS-progress550
tar -tzf progress543-550.tar.gz > "$task_cp/archive-index.txt"
