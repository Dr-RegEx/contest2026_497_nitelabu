#!/bin/bash
# Build only the isolated diagnostic directory; preserve the main Make config.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_build=$task_root/openvela-dev/out/esp32s31-cmake-wifi-cpu0198
task_receipt=${1:?Pass a new receipt path}
test ! -e "$task_receipt"
test -d "$task_root/openvela-dev/apps/crypto/mbedtls/mbedtls"
cd "$task_root/openvela-dev/nuttx"
task_saved=$(mktemp -d /tmp/s31-flat-source-config.XXXXXX)
restore_config() {
  if [ -f "$task_saved/.config" ]; then mv "$task_saved/.config" .config; fi
  if [ -f "$task_saved/config.h" ]; then mv "$task_saved/config.h" include/nuttx/config.h; fi
  rmdir "$task_saved"
}
trap restore_config EXIT
if [ -f .config ]; then mv .config "$task_saved/.config"; fi
if [ -f include/nuttx/config.h ]; then mv include/nuttx/config.h "$task_saved/config.h"; fi
. "$task_root/s31-reference/tmp/esp-idf-clean/export.sh" > "$task_root/backups/2026-09-10-scan-stress/logs/cpu0198-idf-export.log" 2>&1
export PATH="$task_root/backups/2026-09-10-scan-stress/offline-bin:$task_root/s31-reference/.venv-nuttx/bin:$PATH"
export ESP_HAL_3RDPARTY_LOCAL="$task_root/s31-reference/deps/esp-hal-3rdparty"
set -x
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:production-cpu0 \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" --target resetconfig
cmake --build "$task_build" -j8
(set -o noclobber; sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" > "$task_receipt")
