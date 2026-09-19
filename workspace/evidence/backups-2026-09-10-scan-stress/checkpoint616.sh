#!/bin/bash
set -euo pipefail
task_dir=/home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress
cd "$task_dir"
test ! -e progress614-616.tar.gz
test ! -e SHA256SUMS-progress616
sha256sum -c SHA256SUMS-progress613
sha256sum -c build609-irq-mask.sha256
tar -czf progress614-616.tar.gz \
  audit-wifi-osi614.py progress614-osi-audit.md checkpoint616.sh \
  logs/host614-wifi-osi.log logs/host616-wifi-osi-checked.log \
  logs/host615-event-group-code.log logs/host615-idf-retention-code.log \
  logs/http616-readonly-audit.log SHA256SUMS-progress613
sha256sum progress614-616.tar.gz > SHA256SUMS-progress616
sha256sum -c SHA256SUMS-progress616
tar -tzf progress614-616.tar.gz
