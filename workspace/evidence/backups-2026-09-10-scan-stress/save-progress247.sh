#!/bin/bash
# Capture the FPU/config-header investigation without touching references.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_stage=$task_dir/checkpoint247
test ! -e "$task_stage"
test ! -e "$task_dir/progress223-247-checkpoint.tar.gz"
mkdir "$task_stage"
set -x
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_stage/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_stage/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_stage/nuttx-working.patch"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_stage/apps-head.txt"
git -C "$task_root/openvela-dev/apps" status --short > "$task_stage/apps-status.txt"
git -C "$task_root/openvela-dev/out/esp32s31-make-production-verify/nuttx" diff --binary > "$task_stage/make-working.patch"
tar -czf "$task_stage/diagnostic-configs-and-tests.tar.gz" \
  -C "$task_root/openvela-dev/nuttx" \
  .config tools/test_esp32s31_fpu_context.py tools/test_esp32s31_wifi_iram.py \
  tools/test_cmake_config_header.py \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/wifi-flat \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-cpu0 \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-regression
tar -czf "$task_dir/progress223-247-checkpoint.tar.gz" \
  --exclude='logs/*247*' \
  -C "$task_dir" checkpoint247 README.md logs \
  build-regression-wifi.sh build-production-make.sh build-minimal-make.sh \
  flash-regression-pair.sh restore-production180-pair.sh \
  verify-wifi-artifact.py network-probe.py network-repeat.py \
  background-scan-probe.py scan-timeout-probe.py run-redacted.py \
  save-progress247.sh fpu224-eager-both-candidate.patch \
  firmware224-eager-fpu-bgn.tar.gz firmware230-eager-fpu-he-unflashed.tar.gz \
  firmware234-fpu-clean-frame.tar.gz firmware241-clean-fpu-bgn-coherent.tar.gz \
  firmware243-clean-fpu-he-coherent.tar.gz esp32s31-port-66a0-to-19415.bundle \
  esp32s31-port-19415-to-5040.bundle \
  build241.sha256 build243.sha256
cd "$task_dir"
sha256sum progress223-247-checkpoint.tar.gz > SHA256SUMS-progress247
sha256sum -c SHA256SUMS-progress247
tar -tzf progress223-247-checkpoint.tar.gz > "$task_stage/archive-members.txt"
echo PROGRESS247_BACKUP=PASS
