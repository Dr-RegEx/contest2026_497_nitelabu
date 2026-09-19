#!/bin/bash
# Save reproducible evidence without changing Git or protected references.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_stage=$task_dir/checkpoint222
test ! -e "$task_stage"
test ! -e "$task_dir/progress184-222-checkpoint.tar.gz"
mkdir "$task_stage"
set -x
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_stage/nuttx-head.txt"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_stage/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_stage/nuttx-working.patch"
git -C "$task_root/openvela-dev/apps" status --short > "$task_stage/apps-status.txt"
tar -czf "$task_stage/diagnostic-configs-and-tests.tar.gz" \
  -C "$task_root/openvela-dev/nuttx" \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/wifi-flat \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-cpu0 \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-regression \
  tools/test_esp32s31_wifi_iram.py
tar -czf "$task_stage/firmware220-fpu-diagnostic.tar.gz" \
  -C "$task_root/openvela-dev/out/esp32s31-cmake-wifi-regression209" \
  nuttx nuttx.bin nuttx.map .config appfs.img
tar -czf "$task_dir/progress184-222-checkpoint.tar.gz" \
  --exclude='logs/*222*' \
  -C "$task_dir" checkpoint222 README.md logs \
  build-flat-wifi.sh build-cpu0-wifi.sh build-regression-wifi.sh \
  flash-regression-pair.sh restore-production180-pair.sh \
  compare-wifi-init.py flat-boot-probe.py network-probe.py network-repeat.py \
  trapped-instruction.S save-progress222.sh \
  s31-flat190-clic-candidate.patch sleep-defaults205-candidate.patch \
  iram215-candidate.patch appfs-before198.bin \
  appfs-after202-restore.bin appfs-after213-restore.bin \
  firmware198b-cpu0-he.tar.gz firmware209-sleep-defaults-candidate.tar.gz \
  firmware215-iram-candidate.tar.gz
cd "$task_dir"
sha256sum progress184-222-checkpoint.tar.gz > SHA256SUMS-progress222
sha256sum -c SHA256SUMS-progress222
tar -tzf progress184-222-checkpoint.tar.gz > "$task_stage/archive-members.txt"
echo PROGRESS222_BACKUP=PASS
