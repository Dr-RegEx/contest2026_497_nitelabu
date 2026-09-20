#!/bin/bash
set -euo pipefail

task_root=/home/regex/work/esp32s31-openvela
task_dir="$task_root/backups/2026-09-10-scan-stress"
task_build="$task_root/openvela-dev/out/esp32s31-xts-flat-ble-audio-mesh-1727"
task_receipt="${1:?Pass an absolute build receipt path}"
test ! -e "$task_receipt"
cd "$task_root/openvela-dev/nuttx"
task_saved=$(mktemp -d /tmp/s31-ble-audio-mesh-config.XXXXXX)
restore_config()
{
  if [ -f "$task_saved/.config" ]; then mv "$task_saved/.config" .config; fi
  if [ -f "$task_saved/config.h" ]; then mv "$task_saved/config.h" include/nuttx/config.h; fi
  rmdir "$task_saved"
}
trap restore_config EXIT
if [ -f .config ]; then mv .config "$task_saved/.config"; fi
if [ -f include/nuttx/config.h ]; then mv include/nuttx/config.h "$task_saved/config.h"; fi
. "$task_root/s31-reference/tmp/esp-idf-clean/export.sh" > "$task_dir/logs/flat-idf-export.log" 2>&1
export PATH="$task_dir/offline-bin:$task_root/s31-reference/.venv-nuttx/bin:$PATH"
export ESP_HAL_3RDPARTY_LOCAL="$task_root/s31-reference/deps/esp-hal-3rdparty"
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:xts-flat-ble-audio-mesh \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" --target resetconfig
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:xts-flat-ble-audio-mesh \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" -j8
test -f "$task_build/nuttx.bin"
for task_option in BT_AUDIO BT_ISO BT_ISO_PERIPHERAL BT_ISO_CENTRAL \
  BT_ISO_BROADCASTER BT_ISO_SYNC_RECEIVER BT_BAP_UNICAST \
  BT_BAP_UNICAST_CLIENT BT_BAP_BROADCAST_SOURCE \
  BT_MESH BT_MESH_PB_ADV BT_MESH_PB_GATT BT_MESH_PROVISIONEE \
  BT_MESH_RELAY BT_MESH_RELAY_ENABLED BT_MESH_GATT_PROXY; do
  rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
done
for task_symbol in bt_mesh_init bt_iso_chan_connect bt_iso_big_create; do
  rg -q "$task_symbol" "$task_build/nuttx.map"
done
sha256sum "$task_build/nuttx.bin" > "$task_receipt"
printf 'profile=xts-flat-ble-audio-mesh\nimage=%s\nsize=%s\n' \
  "$task_build/nuttx.bin" "$(stat -c %s "$task_build/nuttx.bin")" >> "$task_receipt"
