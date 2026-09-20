#!/bin/bash
set -euo pipefail
project_dir=/home/regex/work/esp32s31-openvela/diagnostics/idf-baseline
export PYTHONDONTWRITEBYTECODE=1
export IDF_COMPONENT_MANAGER=0
export IDF_SKIP_CHECK_SUBMODULES=1
. /home/regex/work/esp32s31-openvela/s31-reference/tmp/esp-idf-clean/export.sh
export PATH=/home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/offline-bin:$PATH
export IDF_TARGET=esp32s31
case "${S31_IDF_VARIANT:-stock}" in
  stock)
    build_dir="$project_dir/build"
    sdkconfig_path="$project_dir/sdkconfig"
    hal_phy=OFF
    hal_wifi=OFF
    ;;
  hal-phy)
    build_dir="$project_dir/build-hal-phy"
    sdkconfig_path="$project_dir/sdkconfig-hal-phy"
    hal_phy=ON
    hal_wifi=OFF
    ;;
  hal-wifi)
    build_dir="$project_dir/build-hal-wifi"
    sdkconfig_path="$project_dir/sdkconfig-hal-wifi"
    hal_phy=ON
    hal_wifi=ON
    ;;
  *) echo "Unknown diagnostic variant" >&2; exit 2 ;;
esac
set -x
cmake -S "$project_dir" -B "$build_dir" -G Ninja \
  -DIDF_TARGET=esp32s31 -DSDKCONFIG="$sdkconfig_path" \
  -DS31_BASELINE_HAL_PHY="$hal_phy" \
  -DS31_BASELINE_HAL_WIFI="$hal_wifi" \
  -DSDKCONFIG_DEFAULTS="$project_dir/sdkconfig.defaults" \
  -DGIT_EXECUTABLE=/home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/offline-bin/git
cmake --build "$build_dir" -j8
sha256sum "$build_dir/bootloader/bootloader.bin" \
  "$build_dir/partition_table/partition-table.bin" \
  "$build_dir/s31_wifi_baseline.bin"
