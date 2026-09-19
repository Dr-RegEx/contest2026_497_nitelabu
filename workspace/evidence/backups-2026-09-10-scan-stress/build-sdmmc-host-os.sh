#!/bin/bash
set -euo pipefail

task_root=/home/regex/work/esp32s31-openvela
task_dir="$task_root/backups/2026-09-10-scan-stress"
task_build="$task_root/openvela-dev/out/esp32s31-xts-flat-sdmmc-host-os-20260919"
task_receipt="${1:?Pass a fresh absolute build receipt path}"
test ! -e "$task_receipt"

cd "$task_root/openvela-dev/nuttx"
task_saved=$(mktemp -d /tmp/s31-ble-mesh-entry-config.XXXXXX)
restore_config()
{
  if [ -f "$task_saved/.config" ]; then mv "$task_saved/.config" .config; fi
  if [ -f "$task_saved/config.h" ]; then mv "$task_saved/config.h" include/nuttx/config.h; fi
  rmdir "$task_saved"
}
trap restore_config EXIT
if [ -f .config ]; then mv .config "$task_saved/.config"; fi
if [ -f include/nuttx/config.h ]; then mv include/nuttx/config.h "$task_saved/config.h"; fi

. "$task_root/s31-reference/tmp/esp-idf-clean/export.sh" > "$task_dir/logs/mesh-host-init-idf-export.log" 2>&1
export PATH="$task_dir/offline-bin:$task_root/s31-reference/.venv-nuttx/bin:$PATH"
export ESP_HAL_3RDPARTY_LOCAL="$task_root/s31-reference/deps/esp-hal-3rdparty"

cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:xts-flat-sdmmc-host \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" --target resetconfig
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:xts-flat-sdmmc-host \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" -j8

test -f "$task_build/nuttx.bin"
rg -q '^CONFIG_ESP32S31_SDMMC_IDF_HOST=y$' "$task_build/.config"
for task_symbol in sdmmc_host_init sdmmc_host_do_transaction xQueueReceive; do
  rg -q " T ${task_symbol}$" "$task_build/System.map"
done

sha256sum "$task_build/nuttx.bin" > "$task_receipt"
printf 'profile=xts-flat-sdmmc-host\nimage=%s\nsize=%s\n' \
  "$task_build/nuttx.bin" "$(stat -c %s "$task_build/nuttx.bin")" >> "$task_receipt"
