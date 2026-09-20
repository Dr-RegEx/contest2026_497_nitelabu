#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_nuttx=$task_root/openvela-dev/nuttx
task_apps=$task_root/openvela-dev/apps
task_build=$task_root/openvela-dev/out/esp32s31-cmake-demo
mkdir "$task_dir/checkpoint281"
cd "$task_dir"
sha256sum -c build272e-demo.sha256
(cd demo-before272 && sha256sum -c SHA256SUMS)
git -C "$task_nuttx" bundle create "$task_dir/esp32s31-port-4282-to-71ec.bundle" 428252a08db4ac461805bee3f79377df7b5dab11..codex/esp32s31-port
git -C "$task_apps" bundle create "$task_dir/esp32s31-apps-c5fb-to-39e6.bundle" c5fba9b030ad2c02ee1f7acb6702a55d363854e2..codex/esp32s31-nettest
git -C "$task_nuttx" bundle verify "$task_dir/esp32s31-port-4282-to-71ec.bundle"
git -C "$task_apps" bundle verify "$task_dir/esp32s31-apps-c5fb-to-39e6.bundle"
git -C "$task_nuttx" diff --binary > checkpoint281/nuttx-working.patch
git -C "$task_nuttx" status --short > checkpoint281/nuttx-status.txt
git -C "$task_apps" status --short > checkpoint281/apps-status.txt
git -C "$task_nuttx" rev-parse HEAD > checkpoint281/nuttx-head.txt
git -C "$task_apps" rev-parse HEAD > checkpoint281/apps-head.txt
tar -czf firmware272e-demo-coherent.tar.gz -C "$task_build" nuttx nuttx.bin nuttx.map .config .config.prev include/nuttx/config.h appfs.img bin/s31demo bin_debug/s31demo
tar -czf checkpoint281/source-profiles.tar.gz -C "$task_nuttx" boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-cpu0 boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-regression boards/risc-v/esp32s31/esp32s31-core-function-board/configs/wifi-flat
tar -czf progress272-281-demo-checkpoint.tar.gz README.md checkpoint281 firmware272e-demo-coherent.tar.gz demo-before272 build272e-demo.sha256 build-demo.sh backup-demo-before.sh flash-demo-pair.sh demo277-restack.py save-demo281.sh esp32s31-port-4282-to-71ec.bundle esp32s31-apps-c5fb-to-39e6.bundle logs/build272* logs/demo272* logs/backup273* logs/flash274* logs/demo275* logs/demo276* logs/demo277* logs/demo278* logs/demo279* logs/demo280*
sha256sum progress272-281-demo-checkpoint.tar.gz firmware272e-demo-coherent.tar.gz esp32s31-port-4282-to-71ec.bundle esp32s31-apps-c5fb-to-39e6.bundle > SHA256SUMS-demo281
sha256sum -c SHA256SUMS-demo281
tar -tzf progress272-281-demo-checkpoint.tar.gz > checkpoint281/archive-members.txt
printf 'DEMO_CHECKPOINT281=PASS\n'
