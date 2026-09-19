#!/bin/bash
set -euo pipefail
cd /home/regex/work/esp32s31-openvela/openvela-dev/nuttx
. /home/regex/work/esp32s31-openvela/s31-reference/tmp/esp-idf-clean/export.sh > /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/logs/make-idf-export.log 2>&1
export PATH=/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin:/home/regex/work/esp32s31-openvela/s31-reference/.venv-nuttx/bin:$PATH
export ESP_HAL_3RDPARTY_LOCAL=/home/regex/work/esp32s31-openvela/s31-reference/deps/esp-hal-3rdparty
set -x
make -j8
