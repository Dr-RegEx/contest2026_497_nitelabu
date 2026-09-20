#!/bin/bash
set -euo pipefail
cd /home/regex/work/esp32s31-openvela/openvela-dev/out/esp32s31-make-production-verify/nuttx
. /home/regex/work/esp32s31-openvela/s31-reference/tmp/esp-idf-clean/export.sh > /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/logs/production-make-idf-export.log 2>&1
export PATH=/home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/offline-bin:/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin:/home/regex/work/esp32s31-openvela/openvela-dev/prebuilts/build-tools/linux-x86_64/bin:/home/regex/work/esp32s31-openvela/s31-reference/.venv-nuttx/bin:$PATH
export ESP_HAL_3RDPARTY_LOCAL=/home/regex/work/esp32s31-openvela/s31-reference/deps/esp-hal-3rdparty
set -x
if [ "${1:-}" = configure ]; then
  tools/configure.sh -l esp32s31-core-function-board:production
elif [ "${1:-}" = export ]; then
  make -j8 export
elif [ "${1:-}" = apps ]; then
  make -C ../apps -j8 import
elif [ "${1:-}" = appfs ]; then
  python3 tools/espressif/esp32s31_prepare_appfs.py --source ../apps/bin --output appfs-root --max-size 524288
  genromfs -f appfs.img -d appfs-root -V S31AppFS
else
  make -j8
fi
