#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint607
test ! -e "$task_cp"
test ! -e "$task_dir/progress600-607.tar.gz"
test ! -e "$task_dir/SHA256SUMS-progress607"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
tar -czf "$task_cp/firmware604.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress599
sha256sum -c build604-restore-bgn.sha256
rg --files logs | rg '(600|601|602|603|604|605|606|http607|host607)' > "$task_cp/log-files.txt"
tar -czf progress600-607.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint607 checkpoint607.sh progress600-network-he.md \
  build601-same-ap-he.sha256 build604-restore-bgn.sha256 \
  firmware601-same-ap-he.tar.gz build-demo.sh flash-he601-pair.sh \
  flash-demo-pair.sh network-probe.py network-repeat.py \
  udp_probe_support.py test-udp-probe.py SHA256SUMS-progress599
sha256sum progress600-607.tar.gz > SHA256SUMS-progress607
sha256sum -c SHA256SUMS-progress607
tar -tzf progress600-607.tar.gz > "$task_cp/archive-index.txt"
