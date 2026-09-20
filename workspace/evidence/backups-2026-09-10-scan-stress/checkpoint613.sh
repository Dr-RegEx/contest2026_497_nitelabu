#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint613
test ! -e "$task_cp"
test ! -e "$task_dir/progress608-613.tar.gz"
test ! -e "$task_dir/SHA256SUMS-progress613"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/irq-mask.bundle" f10a8f4b209..HEAD
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/irq-mask.bundle"
tar -czf "$task_cp/firmware609.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress607
sha256sum -c build609-irq-mask.sha256
rg --files logs | rg '(608|609|610|611|612|http613)' > "$task_cp/log-files.txt"
tar -czf progress608-613.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint613 checkpoint613.sh progress608-irq-mask.md irq-mask608-index.patch \
  build609-irq-mask.sha256 build-demo.sh flash-demo-pair.sh \
  network-probe.py network-repeat.py SHA256SUMS-progress607
sha256sum progress608-613.tar.gz > SHA256SUMS-progress613
sha256sum -c SHA256SUMS-progress613
tar -tzf progress608-613.tar.gz > "$task_cp/archive-index.txt"
