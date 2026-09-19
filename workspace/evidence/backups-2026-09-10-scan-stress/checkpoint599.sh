#!/bin/bash
set -euo pipefail
task_dir=/home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress
cd "$task_dir"
test ! -e progress595-599.tar.gz
test ! -e SHA256SUMS-progress599
sha256sum -c SHA256SUMS-progress594
sha256sum -c build591-arp-callback.sha256
tar -czf progress595-599.tar.gz \
  progress595-same-ap.md checkpoint599.sh SHA256SUMS-progress594 \
  build551-clean.sha256 build591-arp-callback.sha256 \
  network-repeat.py network-probe.py tcp-nettest-client.py \
  flash-clean551.sh flash-demo-pair.sh \
  logs/flash595-clean-same-ap.log logs/host596-same-ap.log \
  logs/network596-clean-same-ap-cold-1-redacted.log \
  logs/network596-clean-same-ap-cold-2-redacted.log \
  logs/network596-clean-same-ap-cold-3-redacted.log \
  logs/flash597-restore-demo.log logs/demo598-same-ap.log \
  logs/http599-same-ap.log
sha256sum progress595-599.tar.gz > SHA256SUMS-progress599
sha256sum -c SHA256SUMS-progress599
tar -tzf progress595-599.tar.gz
