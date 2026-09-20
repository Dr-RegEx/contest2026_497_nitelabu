#!/bin/bash
set -euo pipefail

task_root=/home/regex/work/esp32s31-openvela
task_build=$task_root/openvela-dev/out/esp32s31-ble-static1725
task_profile=demo-rmt-bttool-kernel
task_receipt=$(realpath -m "${1:?Pass a fresh paired receipt}")
test ! -e "$task_receipt"

cd "$task_root/openvela-dev/nuttx"
task_saved=$(mktemp -d /tmp/s31-ble-static-config.XXXXXX)
restore_config()
{
  if [ -f "$task_saved/.config" ]; then mv "$task_saved/.config" .config; fi
  if [ -f "$task_saved/config.h" ]; then mv "$task_saved/config.h" include/nuttx/config.h; fi
  rmdir "$task_saved"
}
trap restore_config EXIT
if [ -f .config ]; then mv .config "$task_saved/.config"; fi
if [ -f include/nuttx/config.h ]; then mv include/nuttx/config.h "$task_saved/config.h"; fi

. "$task_root/s31-reference/tmp/esp-idf-clean/export.sh" > "$task_root/backups/2026-09-10-scan-stress/logs/ble1725-idf-export.log" 2>&1
export PATH="$task_root/backups/2026-09-10-scan-stress/offline-bin:$task_root/s31-reference/.venv-nuttx/bin:$PATH"
export ESP_HAL_3RDPARTY_LOCAL="$task_root/s31-reference/deps/esp-hal-3rdparty"

cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:"$task_profile" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" --target resetconfig
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:"$task_profile" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON

for task_option in BUILD_KERNEL SMP ESP32S31_SMP ARCH_ADDRENV ESP32S31_BLE \
  BLUETOOTH_TOOLS BLUETOOTH_BLE_SUPPORT BLUETOOTH_BLE_ADV \
  BLUETOOTH_BLE_SCAN BLUETOOTH_GATT_SERVER BT_HCI_HOST BT_PERIPHERAL \
  BT_CENTRAL BT_BROADCASTER BT_OBSERVER BT_EXT_ADV BT_PHY_UPDATE \
  BT_DATA_LEN_UPDATE BT_SMP; do
  rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
done

cmake --build "$task_build" -j8
test -f "$task_build/appfs-root/bttool"
test -f "$task_build/nuttx.bin"
test -f "$task_build/appfs.img"
set -o noclobber
sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" > "$task_receipt"
