#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint640
test ! -e "$task_cp"
test ! -e "$task_dir/progress632-640.tar.gz"
test ! -e "$task_dir/SHA256SUMS-progress640"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/event-board.bundle" 15cb4c646f3..HEAD
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/event-board.bundle"
tar -czf "$task_cp/firmware635.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-cmake-demo" \
  nuttx nuttx.bin appfs.img .config
cd "$task_dir"
sha256sum -c SHA256SUMS-progress631
sha256sum -c build635-event-restored.sha256
rg --files logs | rg '(63[2-9])' > "$task_cp/log-files.txt"
tar -czf progress632-640.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint640 checkpoint640.sh progress632-event-board.md event634-boot.py \
  build632b-event-board.sha256 build635-event-restored.sha256 \
  firmware632b-event-board.tar.gz build-demo.sh flash-demo-pair.sh \
  SHA256SUMS-progress631
sha256sum progress632-640.tar.gz > SHA256SUMS-progress640
sha256sum -c SHA256SUMS-progress640
tar -tzf progress632-640.tar.gz > "$task_cp/archive-index.txt"
