#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint470
test ! -e "$task_cp"
test ! -e "$task_dir/progress458-470.tar.gz"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
git -C "$task_root/openvela-dev/apps" rev-parse HEAD > "$task_cp/apps-head.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
tar -czf "$task_cp/nuttx-untracked-profiles-tests.tar.gz" -C "$task_root/openvela-dev/nuttx" \
  tools/test_esp32s31_wifi_latency.py tools/test_esp32s31_tcp_synack_diag.py \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-rmt-he \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-rmt-tcpdiag \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-rmt-arp-learn
cd "$task_dir"
sha256sum -c SHA256SUMS-progress457
git -C "$task_root/openvela-dev/nuttx" bundle verify \
  "$task_dir/checkpoint457/nuttx-ab5a-9d3b.bundle"
rg --files logs | rg '(45[8-9]|46[0-9]|470)' | rg -v '/checkpoint' > "$task_cp/log-files.txt"
tar -czf progress458-470.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint470 checkpoint470.sh progress470-arp-isolation.md \
  firmware458-tcp-proc.tar.gz firmware462-tcp-synack.tar.gz firmware469-arp-learn.tar.gz \
  build458-tcp-proc.sha256 build462-tcp-synack.sha256 build469-arp-learn.sha256 \
  diagnostics469.patch build-demo.sh flash-demo-pair.sh \
  network-probe.py network-repeat.py tcp-nettest-client.py \
  SHA256SUMS-progress457
sha256sum progress458-470.tar.gz > SHA256SUMS-progress470
sha256sum -c SHA256SUMS-progress470
tar -tzf progress458-470.tar.gz > "$task_cp/archive-index.txt"
