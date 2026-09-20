#!/usr/bin/env bash
set -euo pipefail
root=$(realpath "${1:?Usage: build.sh /path/to/workspace [profile] [jobs]}")
profile=${2:-nsh}
[[ "$profile" =~ ^[a-zA-Z0-9_-]+$ ]] || exit 2
export ESP_HAL_3RDPARTY_LOCAL="$root/.s31-deps/esp-hal-3rdparty"
export PATH="$root/.s31-deps/venv/bin:$root/.s31-deps/riscv32-esp-elf/bin:$PATH"
command -v riscv32-esp-elf-gcc
command -v olddefconfig
# In-tree generated headers override out-of-tree headers; reject mixed builds.
if [[ -e "$root/nuttx/.config" || -e "$root/nuttx/include/nuttx/config.h" ]]; then
  echo 'Use a fresh source workspace without in-tree .config/config.h' >&2
  exit 1
fi
out="$root/out/esp32s31-$profile"
cmake -S "$root/nuttx" -B "$out" -G Ninja \
  -DBOARD_CONFIG="esp32s31-core-function-board:$profile"
cmake --build "$out" -j "${3:-8}"
