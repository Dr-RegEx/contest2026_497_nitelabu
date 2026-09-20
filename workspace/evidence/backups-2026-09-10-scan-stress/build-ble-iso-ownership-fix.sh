#!/bin/bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
out=$root/openvela-dev/out/esp32s31-ble-iso-ownership-fix
save=$(mktemp -d /tmp/s31-iso-bridge-source.XXXXXX)
restore(){ test ! -e "$save/.config" || mv "$save/.config" "$root/openvela-dev/nuttx/.config"; test ! -e "$save/config.h" || mv "$save/config.h" "$root/openvela-dev/nuttx/include/nuttx/config.h"; rmdir "$save" 2>/dev/null || true; }
trap restore EXIT
if test -e "$root/openvela-dev/nuttx/.config"; then mv "$root/openvela-dev/nuttx/.config" "$save/.config"; fi
if test -e "$root/openvela-dev/nuttx/include/nuttx/config.h"; then mv "$root/openvela-dev/nuttx/include/nuttx/config.h" "$save/config.h"; fi
source "$root/s31-reference/tmp/esp-idf-clean/export.sh" >/tmp/s31-iso-bridge-export.log 2>&1
export PATH="$root/backups/2026-09-10-scan-stress/offline-bin:$root/s31-reference/.venv-nuttx/bin:$PATH"
export ESP_HAL_3RDPARTY_LOCAL="$root/s31-reference/deps/esp-hal-3rdparty"
cmake -S "$root/openvela-dev/nuttx" -B "$out" -G Ninja -DBOARD_CONFIG=esp32s31-core-function-board:xts-flat-ble-audio-mesh -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$out" -j8
