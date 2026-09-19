#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint520
test ! -e "$task_cp"
test ! -e "$task_dir/progress506-520.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_cp/apps-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/nuttx-after21d0.bundle" \
  refs/heads/codex/esp32s31-port ^21d0fd73f0cb0eddfcf6616746abbb156f6f27eb
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/nuttx-after21d0.bundle"
git -C "$task_root/openvela-dev/nuttx" ls-files --others --exclude-standard -z > "$task_cp/untracked.list"
tar -czf "$task_cp/nuttx-untracked.tar.gz" -C "$task_root/openvela-dev/nuttx" --null -T "$task_cp/untracked.list"
tar -czf "$task_cp/firmware517.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress505
rg --files logs | rg '(50[6-9]|51[0-9]|520)' > "$task_cp/log-files.txt"
tar -czf progress506-520.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint520 checkpoint520.sh progress513-arp-queue.md resume506.md resume520.md \
  firmware513-arp-queue.tar.gz diagnostics513-arp-queue.patch config478-to513.diff \
  build509-arp-queue.sha256 build513-arp-expiry.sha256 build517-demo-arp-fixes.sha256 \
  build-demo.sh flash-demo-pair.sh network-probe.py network-repeat.py \
  tcp-nettest-client.py SHA256SUMS-progress505
sha256sum progress506-520.tar.gz > SHA256SUMS-progress520
sha256sum -c SHA256SUMS-progress520
tar -tzf progress506-520.tar.gz > "$task_cp/archive-index.txt"
