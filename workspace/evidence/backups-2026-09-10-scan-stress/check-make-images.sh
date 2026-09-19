#!/bin/bash
set -euo pipefail
cd /home/regex/work/esp32s31-openvela/openvela-dev/out/esp32s31-make-production-verify/nuttx
set -x
test "$(stat -c %s nuttx.bin)" -le $((0x200000 - 0x2000))
test "$(stat -c %s appfs.img)" -le $((0x300000))
test -f appfs-root/init
for app in init sh wapi ping renew; do
  test "$(stat -c %s "appfs-root/$app")" -le 524288
  test "$(sha256sum "appfs-root/$app" | cut -d ' ' -f 1)" = "$(head -1 "appfs-root/$app.sha256")"
done
/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool --chip esp32s31 image-info nuttx.bin
sha256sum nuttx.bin appfs.img
printf 'MAKE_IMAGE_BOUNDARIES_AND_APP_HASHES=PASS\n'
