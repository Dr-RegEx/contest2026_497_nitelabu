#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_source=$task_root/openvela-dev/out/esp32s31-clean551/nuttx
task_build=$task_root/openvela-dev/out/esp32s31-clean551/build
task_receipt=$task_dir/build551-clean.sha256
test ! -e "$task_receipt"
test "$(git -C "$task_source" rev-parse --short=11 HEAD)" = 39be74d0093
git -C "$task_source" diff --exit-code
test -d "$task_source/fs/littlefs/littlefs"
. "$task_root/s31-reference/tmp/esp-idf-clean/export.sh" > "$task_dir/logs/clean551-idf-export.log" 2>&1
export PATH="$task_dir/offline-bin:$task_root/s31-reference/.venv-nuttx/bin:$PATH"
export ESP_HAL_3RDPARTY_LOCAL="$task_root/s31-reference/deps/esp-hal-3rdparty"
cd "$task_source"
set -x
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:demo-rmt-tcpdiag \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
rg -q '^CONFIG_NET_STATISTICS=y$' "$task_build/.config"
for task_option in ESPRESSIF_WIFI_STA_11AX NET_ARP_IPIN NET_ARP_SEND_QUEUE; do
  rg -q "^# CONFIG_${task_option} is not set$" "$task_build/.config"
done
cmake --build "$task_build" -j8
python3 "$task_dir/verify-wifi-artifact.py" "$task_build"
test -f "$task_build/appfs-root/s31demo"
test -f "$task_build/appfs-root/s31led"
(set -o noclobber; sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" > "$task_receipt")
