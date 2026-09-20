#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_nuttx=$task_root/openvela-dev/nuttx
task_apps=$task_root/openvela-dev/apps
task_build=$task_root/openvela-dev/out/esp32s31-cmake-demo
cd "$task_dir"
mkdir checkpoint290
sha256sum -c build284-demo-metadata.sha256
git -C "$task_nuttx" bundle create "$task_dir/esp32s31-port-71ec-to-4226.bundle" 71ecca5e28330c42697e70b226338eb437fa2a9e..codex/esp32s31-port
git -C "$task_apps" bundle create "$task_dir/esp32s31-apps-39e6-to-118d.bundle" 39e68c3346b3c1f196d8b3e07615c057da73dbf4..codex/esp32s31-nettest
git -C "$task_nuttx" bundle verify "$task_dir/esp32s31-port-71ec-to-4226.bundle"
git -C "$task_apps" bundle verify "$task_dir/esp32s31-apps-39e6-to-118d.bundle"
git -C "$task_nuttx" diff --binary > checkpoint290/nuttx-working.patch
git -C "$task_nuttx" status --short > checkpoint290/nuttx-status.txt
git -C "$task_apps" status --short > checkpoint290/apps-status.txt
git -C "$task_nuttx" rev-parse HEAD > checkpoint290/nuttx-head.txt
git -C "$task_apps" rev-parse HEAD > checkpoint290/apps-head.txt
tar -czf firmware284-demo-metadata-coherent.tar.gz -C "$task_build" nuttx nuttx.bin nuttx.map .config .config.prev include/nuttx/config.h appfs.img bin/s31demo bin_debug/s31demo
tar -czf progress282-290-demo-checkpoint.tar.gz README.md checkpoint290 firmware284-demo-metadata-coherent.tar.gz build284-demo-metadata.sha256 demo287-default-stack.py save-demo290.sh esp32s31-port-71ec-to-4226.bundle esp32s31-apps-39e6-to-118d.bundle logs/demo282* logs/demo283* logs/build284* logs/demo284* logs/demo285* logs/flash286* logs/demo287* logs/demo288* logs/host289*
sha256sum progress282-290-demo-checkpoint.tar.gz firmware284-demo-metadata-coherent.tar.gz esp32s31-port-71ec-to-4226.bundle esp32s31-apps-39e6-to-118d.bundle > SHA256SUMS-demo290
sha256sum -c SHA256SUMS-demo290
printf 'DEMO_CHECKPOINT290=PASS\n'
