#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint491
test ! -e "$task_cp"
test ! -e "$task_dir/progress471-491.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_cp/apps-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/nuttx-9d3b-532a.bundle" \
  refs/heads/codex/esp32s31-port ^9d3b75798a3d238a02bb6df40898ae4db741045a
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/nuttx-9d3b-532a.bundle"
git -C "$task_root/openvela-dev/nuttx" ls-files --others --exclude-standard -z > "$task_cp/untracked.list"
tar -czf "$task_cp/nuttx-untracked.tar.gz" -C "$task_root/openvela-dev/nuttx" --null -T "$task_cp/untracked.list"
tar -czf "$task_cp/firmware489.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress470
rg --files logs | rg '(47[1-9]|48[0-9]|49[01])' > "$task_cp/log-files.txt"
tar -czf progress471-491.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint491 checkpoint491.sh progress491-rmt-write.md progress480-arp-boundary.md \
  dependency476-nested-hal-audit.md hal-mbedtls-existing-476.patch \
  firmware473-arp-learn-he.tar.gz firmware478-arp-trace.tar.gz firmware482-padding.tar.gz \
  wlan-padding482-experiment.tar.gz diagnostics480.patch diagnostics482-padding.patch \
  build473-arp-learn-he.sha256 build478-arp-trace.sha256 build482-wlan-padding.sha256 \
  build489-rmt-write.sha256 build-demo.sh flash-demo-pair.sh flash-arp-he-pair.sh \
  network-probe.py network-repeat.py tcp-nettest-client.py SHA256SUMS-progress470
sha256sum progress471-491.tar.gz > SHA256SUMS-progress491
sha256sum -c SHA256SUMS-progress491
tar -tzf progress471-491.tar.gz > "$task_cp/archive-index.txt"
