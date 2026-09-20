#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint528
test ! -e "$task_cp"
test ! -e "$task_dir/progress521-528.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_cp/apps-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" ls-files --others --exclude-standard -z > "$task_cp/untracked.list"
tar -czf "$task_cp/nuttx-untracked.tar.gz" -C "$task_root/openvela-dev/nuttx" --null -T "$task_cp/untracked.list"
tar -czf "$task_cp/firmware525.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress520
rg --files logs | rg '52[1-8]' > "$task_cp/log-files.txt"
tar -czf progress521-528.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint528 checkpoint528.sh progress522-arp-txdone.md \
  firmware522-arp-txdone.tar.gz diagnostics522-arp-txdone.patch \
  build522-arp-txdone.sha256 build525-restore-demo.sha256 \
  build-demo.sh flash-demo-pair.sh network-probe.py network-repeat.py \
  tcp-nettest-client.py SHA256SUMS-progress520
sha256sum progress521-528.tar.gz > SHA256SUMS-progress528
sha256sum -c SHA256SUMS-progress528
tar -tzf progress521-528.tar.gz > "$task_cp/archive-index.txt"
