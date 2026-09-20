#!/bin/bash
set -euo pipefail

task_root=/home/regex/work/esp32s31-openvela
task_dir="$task_root/backups/2026-09-10-scan-stress"
task_build="$task_root/openvela-dev/out/esp32s31-xts-flat-ble-mesh-host-init-20260919"
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
  -DBOARD_CONFIG=esp32s31-core-function-board:xts-flat-ble-mesh-init \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" --target resetconfig
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:xts-flat-ble-mesh-init \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" -j8

test -f "$task_build/nuttx.bin"
for task_option in BT_MESH BT_MESH_PB_ADV BT_MESH_PB_GATT \
  BT_MESH_PROVISIONEE BT_MESH_RELAY BT_MESH_GATT_PROXY BT_MESH_SHELL; do
  rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
done
rg -q '^CONFIG_BT_COMPANY_ID=0x05F1$' "$task_build/.config"
rg -q 'mesh_shell/src/main.c.o' "$task_build/build.ninja"
for task_symbol in mesh_main bt_enable_mc bt_mesh_init; do
  rg -q "[[:space:]]${task_symbol}$" "$task_build/System.map"
done

# Recompile the application entry with the port-wide warning suppression
# overridden.  This proves the bt_ready callback matches bt_ready_cb_t.
python3 - "$task_build" <<'PY'
import json
import pathlib
import shlex
import subprocess
import sys

build = pathlib.Path(sys.argv[1])
entries = json.loads((build / "compile_commands.json").read_text())
entry = next(e for e in entries if e["file"].endswith(
    "/tests/bluetooth/mesh_shell/src/main.c"))
cmd = shlex.split(entry["command"])
out = pathlib.Path("/tmp/s31-mesh-entry-strict.o")
cmd[cmd.index("-o") + 1] = str(out)
cmd.extend(["-Wincompatible-pointer-types", "-Werror=incompatible-pointer-types"])
subprocess.run(cmd, cwd=entry["directory"], check=True)
if not out.is_file() or out.stat().st_size == 0:
    raise SystemExit("strict Mesh entry object missing")
PY

sha256sum "$task_build/nuttx.bin" > "$task_receipt"
printf 'profile=xts-flat-ble-mesh-init\nimage=%s\nsize=%s\n' \
  "$task_build/nuttx.bin" "$(stat -c %s "$task_build/nuttx.bin")" >> "$task_receipt"
