#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint457
test ! -e "$task_cp/nuttx-ab5a-9d3b.bundle"
test ! -e "$task_dir/progress434-457.tar.gz"
mkdir -p "$task_cp"
test "$(git -C "$task_root/openvela-dev/nuttx" rev-parse --short=11 HEAD)" = 9d3b75798a3
git -C "$task_root/openvela-dev/nuttx" bundle create "$task_cp/nuttx-ab5a-9d3b.bundle" codex/esp32s31-port ^ab5a407affe
git -C "$task_root/openvela-dev/nuttx" bundle verify "$task_cp/nuttx-ab5a-9d3b.bundle"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/apps" status --short > "$task_cp/apps-status.txt"
git -C "$task_root/openvela-dev/nuttx" diff --binary > "$task_cp/nuttx-diagnostics.patch"
tar -czf "$task_cp/nuttx-untracked-profiles-tests.tar.gz" -C "$task_root/openvela-dev/nuttx" \
  tools/test_esp32s31_wifi_latency.py \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-rmt-he \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-rmt-tcpdiag \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-cpu0 \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-regression \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/wifi-flat
cd "$task_dir"
rg --files logs | rg '(43[4-9]|44[0-9]|45[0-7])' > "$task_cp/log-files.txt"
tar -czf progress434-457.tar.gz -T "$task_cp/log-files.txt" \
  checkpoint457 progress457-tcp-context.md checkpoint457.sh \
  firmware447-latency-split.tar.gz firmware451-he-latency-split.tar.gz \
  firmware454-tcp-accept-fixed.tar.gz \
  build443-wifi-latency-bgn.sha256 build447-wifi-latency-split.sha256 \
  build451-he-latency-split.sha256 build454-demo.sha256 \
  diagnostics-before442.patch diagnostics447-latency-split.patch diagnostics457.patch \
  build-demo.sh flash-demo-pair.sh flash-make434-pair.sh irq-idle441.py \
  network-probe.py network-repeat.py tcp-nettest-client.py
sha256sum progress434-457.tar.gz > SHA256SUMS-progress457
sha256sum -c SHA256SUMS-progress457
tar -tzf progress434-457.tar.gz > "$task_cp/archive-index.txt"
