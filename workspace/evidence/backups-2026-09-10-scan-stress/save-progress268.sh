#!/bin/bash
# Preserve the verified changes and the separate, uncommitted timing probe.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_stage=$task_dir/checkpoint268
test ! -e "$task_stage"
test ! -e "$task_dir/progress249-268-checkpoint.tar.gz"
mkdir "$task_stage"
set -x
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_stage/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_stage/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_stage/nuttx-working.patch"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_stage/apps-head.txt"
git -C "$task_root/openvela-dev/apps" status --short > "$task_stage/apps-status.txt"
git -C "$task_root/openvela-dev/out/esp32s31-make-production-verify/nuttx" diff --binary > "$task_stage/make-working.patch"
tar -czf "$task_stage/diagnostic-configs.tar.gz" \
  -C "$task_root/openvela-dev/nuttx" .config \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/wifi-flat \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-cpu0 \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-regression
tar -czf "$task_dir/progress249-268-checkpoint.tar.gz" \
  --exclude='logs/*268*' \
  -C "$task_dir" checkpoint268 README.md logs \
  build-regression-wifi.sh build-production-make.sh build-minimal-make.sh \
  flash-regression-pair.sh restore-production180-pair.sh \
  verify-wifi-artifact.py network-probe.py network-repeat.py \
  save-progress268.sh monlat262-diagnostic.patch heap-callbacks-index.patch \
  firmware249-clean-iram-he-coherent.tar.gz firmware253-clean-iram-bgn-coherent.tar.gz \
  firmware257-heap-bgn-coherent.tar.gz firmware262-monlat-bgn-coherent.tar.gz \
  firmware265-monlat-he-coherent.tar.gz \
  esp32s31-port-5040-to-60af.bundle esp32s31-port-60af-to-4282.bundle \
  build249.sha256 build253.sha256 build257.sha256 build262.sha256 build265.sha256
cd "$task_dir"
sha256sum progress249-268-checkpoint.tar.gz > SHA256SUMS-progress268
sha256sum -c SHA256SUMS-progress268
tar -tzf progress249-268-checkpoint.tar.gz > "$task_stage/archive-members.txt"
echo PROGRESS268_BACKUP=PASS
