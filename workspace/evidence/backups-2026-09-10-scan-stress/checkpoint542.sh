#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint542
test ! -e "$task_cp"
test ! -e "$task_dir/progress529-542.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_cp/apps-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/nuttx-aftercc3d.bundle" \
  refs/heads/codex/esp32s31-port ^cc3da0ce1010033a5cfceb6cff3de35282014217
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/nuttx-aftercc3d.bundle"
git -C "$task_root/openvela-dev/nuttx" ls-files --others --exclude-standard -z > "$task_cp/untracked.list"
tar -czf "$task_cp/nuttx-untracked.tar.gz" -C "$task_root/openvela-dev/nuttx" --null -T "$task_cp/untracked.list"
tar -czf "$task_cp/firmware539.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress528
rg --files logs | rg '(529|53[0-9]|54[0-2])' > "$task_cp/log-files.txt"
tar -czf progress529-542.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint542 checkpoint542.sh progress536-callback-format.md rgb533-optical-acceptance.md \
  firmware530-tx-shape.tar.gz firmware536-arp-prefix.tar.gz \
  diagnostics530-tx-shape.patch diagnostics536-arp-prefix.patch \
  build530-tx-shape.sha256 build536-arp-prefix.sha256 build539-restore-demo.sha256 \
  build-demo.sh flash-demo-pair.sh network-probe.py network-repeat.py \
  tcp-nettest-client.py SHA256SUMS-progress528
sha256sum progress529-542.tar.gz > SHA256SUMS-progress542
sha256sum -c SHA256SUMS-progress542
tar -tzf progress529-542.tar.gz > "$task_cp/archive-index.txt"
