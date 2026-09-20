#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint557
test ! -e "$task_cp"
test ! -e "$task_dir/progress551-557.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_cp/apps-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
git -C "$task_root/openvela-dev/nuttx" worktree list > "$task_cp/worktrees.txt"
git -C "$task_root/openvela-dev/out/esp32s31-clean551/nuttx" diff --exit-code
tar -czf "$task_cp/firmware551.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-clean551/build" \
  nuttx nuttx.bin appfs.img .config
tar -czf "$task_cp/clean551-profile.tar.gz" -C "$task_root/openvela-dev/out/esp32s31-clean551/nuttx" \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-rmt-tcpdiag
cd "$task_dir"
sha256sum -c SHA256SUMS-progress550
rg --files logs | rg '55[1-7]' > "$task_cp/log-files.txt"
tar -czf progress551-557.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint557 checkpoint557.sh progress551-clean-control.md \
  config544-to551.diff build551-clean.sha256 build-clean551.sh flash-clean551.sh \
  build547-restore-demo.sha256 flash-demo-pair.sh network-probe.py network-repeat.py \
  tcp-nettest-client.py SHA256SUMS-progress550
sha256sum progress551-557.tar.gz > SHA256SUMS-progress557
sha256sum -c SHA256SUMS-progress557
tar -tzf progress551-557.tar.gz > "$task_cp/archive-index.txt"
