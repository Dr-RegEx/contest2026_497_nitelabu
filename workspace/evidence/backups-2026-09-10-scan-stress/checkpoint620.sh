#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_cp=$task_dir/checkpoint620
test ! -e "$task_cp"
test ! -e "$task_dir/progress617-620.tar.gz"
test ! -e "$task_dir/SHA256SUMS-progress620"
mkdir "$task_cp"
git -C "$task_root/openvela-dev/nuttx" status --short > "$task_cp/nuttx-status.txt"
git -C "$task_root/openvela-dev/nuttx" rev-parse HEAD > "$task_cp/nuttx-head.txt"
tar -czf "$task_cp/event-candidate.tar.gz" -C "$task_root/openvela-dev/nuttx" \
  arch/risc-v/src/esp32s31/esp32s31_wifi_event.c \
  arch/risc-v/src/esp32s31/esp32s31_wifi_event.h \
  tools/test_esp32s31_wifi_event.py
cd "$task_dir"
sha256sum -c SHA256SUMS-progress616
sha256sum -c build609-irq-mask.sha256
tar -czf progress617-620.tar.gz checkpoint620 checkpoint620.sh \
  progress617-event-candidate.md check-event619.py \
  logs/host617-wifi-event.log logs/host618-wifi-event.log \
  logs/host619-target-event-syntax.log SHA256SUMS-progress616
sha256sum progress617-620.tar.gz > SHA256SUMS-progress620
sha256sum -c SHA256SUMS-progress620
tar -tzf progress617-620.tar.gz > "$task_cp/archive-index.txt"
