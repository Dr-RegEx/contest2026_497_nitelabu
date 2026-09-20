#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
out="$root/openvela-dev/out/esp32s31-xts-flat-usb-adb"
json="$out/compile_commands.json"
source="$root/backups/2026-09-10-scan-stress/sd1757-host-init-candidate.c"

if [[ ! -f "$json" ]]; then
  echo "missing compile database: $json" >&2
  exit 2
fi

cmd=$(perl -ne '
  if (index($_, "esp32s31_usbdev.c\"") >= 0 &&
      /"command": "(.*)",\s*$/) {
    print "$1\n";
    exit;
  }
' "$json")

if [[ -z "$cmd" ]]; then
  echo "no S31 compile command found" >&2
  exit 2
fi

cmd=$(printf '%s' "$cmd" | sed "s#${root}/openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_usbdev.c#${source}#g")
cmd=$(printf '%s' "$cmd" | sed 's#-o [^ ]*esp32s31_usbdev.c.o#-o /tmp/sd1757-host-init-candidate.o#')
cmd="$cmd -fsyntax-only"
cmd="$cmd -I$root/openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp-hal-3rdparty/components/upper_hal_sdmmc/legacy/include"
cmd="$cmd -I$root/openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp-hal-3rdparty/components/upper_hal_sdmmc/include"
cmd="$cmd -I$root/openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp-hal-3rdparty/components/upper_hal_sd_intf/include"
cmd="$cmd -I$root/esp-idf-v6.1-upstream/components/sdmmc/include"
cmd="$cmd -I$root/openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp-hal-3rdparty/components/esp_hal_sd/include"
cmd="$cmd -I$root/openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp-hal-3rdparty/components/esp_hal_sd/esp32s31/include"
cmd="$cmd -I$root/s31-reference/deps/esp-hal-3rdparty/components/hal/include"
cmd="$cmd -I$root/s31-reference/deps/esp-hal-3rdparty/components/hal/esp32s31/include"
cmd="$cmd -I$root/s31-reference/deps/esp-hal-3rdparty/components/soc/include"
cmd="$cmd -I$root/s31-reference/deps/esp-hal-3rdparty/components/soc/esp32s31/include"

eval "$cmd"
echo "SD1757_HOST_INIT_SYNTAX=PASS"
