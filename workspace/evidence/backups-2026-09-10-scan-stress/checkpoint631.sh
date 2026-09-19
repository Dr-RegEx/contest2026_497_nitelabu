#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint631
test ! -e "$task_cp"
test ! -e "$task_dir/progress621-631.tar.gz"
test ! -e "$task_dir/SHA256SUMS-progress631"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/event-groups.bundle" f65fe96c6d2..HEAD
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/event-groups.bundle"
tar -czf "$task_cp/firmware623.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress620
sha256sum -c build623-wifi-event.sha256
rg --files logs | rg '(62[1-9]|630|http631)' > "$task_cp/log-files.txt"
tar -czf progress621-631.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint631 checkpoint631.sh progress621-event-integration.md \
  event627-index.py event627-index.patch audit-wifi-osi614.py check-event619.py \
  build623-wifi-event.sha256 build-demo.sh flash-demo-pair.sh \
  network-probe.py network-repeat.py SHA256SUMS-progress620
sha256sum progress621-631.tar.gz > SHA256SUMS-progress631
sha256sum -c SHA256SUMS-progress631
tar -tzf progress621-631.tar.gz > "$task_cp/archive-index.txt"
