#!/bin/bash
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_build=$task_root/openvela-dev/out/esp32s31-spawn-kernel1720
task_profile=demo-rmt-bttool-coex-pie-netdiag-sack-window128-ampdu12
task_receipt=${1:?Pass a fresh paired receipt}
test ! -e "$task_receipt"
cd "$task_root/openvela-dev/nuttx"
task_saved=$(mktemp -d /tmp/s31-demo-source-config.XXXXXX)
restore_config() { if [ -f "$task_saved/.config" ]; then mv "$task_saved/.config" .config; fi; if [ -f "$task_saved/config.h" ]; then mv "$task_saved/config.h" include/nuttx/config.h; fi; rmdir "$task_saved"; }
trap restore_config EXIT
if [ -f .config ]; then mv .config "$task_saved/.config"; fi
if [ -f include/nuttx/config.h ]; then mv include/nuttx/config.h "$task_saved/config.h"; fi
. "$task_root/s31-reference/tmp/esp-idf-clean/export.sh" > "$task_root/backups/2026-09-10-scan-stress/logs/demo-idf-export.log" 2>&1
export PATH="$task_root/backups/2026-09-10-scan-stress/offline-bin:$task_root/s31-reference/.venv-nuttx/bin:$PATH"
export ESP_HAL_3RDPARTY_LOCAL="$task_root/s31-reference/deps/esp-hal-3rdparty"
cmake -S . -B "$task_build" -G Ninja -DBOARD_CONFIG=esp32s31-core-function-board:"$task_profile" -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" --target resetconfig
cmake -S . -B "$task_build" -G Ninja -DBOARD_CONFIG=esp32s31-core-function-board:"$task_profile" -DFETCHCONTENT_FULLY_DISCONNECTED=ON
for task_option in NET_TCP_SELECTIVE_ACK NET_TCP_OUT_OF_ORDER NET_TCP_WINDOW_SCALE ESPRESSIF_WIFI_AMPDU_RX_ENABLED ESPRESSIF_WIFI_AMPDU_TX_ENABLED; do rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"; done
rg -q '^CONFIG_ESPRESSIF_WIFI_RX_BA_WIN=12$' "$task_build/.config"
rg -q '^CONFIG_ESPRESSIF_WIFI_TX_BA_WIN=12$' "$task_build/.config"
rg -q '^CONFIG_ESPRESSIF_WIFI_STATIC_RX_BUFFER_NUM=12$' "$task_build/.config"
cmake --build "$task_build" -j8
test -f "$task_build/nuttx.bin"; test -f "$task_build/appfs.img"
test "$(stat -c %s "$task_build/nuttx.bin")" -le 2088960
test "$(stat -c %s "$task_build/appfs.img")" -le 3145728
set -o noclobber
sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" > "$task_receipt"
