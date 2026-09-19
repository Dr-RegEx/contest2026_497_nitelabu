#!/bin/bash
# Preserve the GPIO/button and IRQ groups before proceeding to I2C.
set -euo pipefail
set -o noclobber
task_root=/home/regex/work/esp32s31-openvela
task_dir=$task_root/backups/2026-09-10-scan-stress
task_nuttx=$task_root/openvela-dev/nuttx
task_apps=$task_root/openvela-dev/apps
task_build=$task_root/openvela-dev/out/esp32s31-cmake-demo
cd "$task_dir"
for task_name in checkpoint325 progress291-325-checkpoint.tar.gz \
  firmware324-demo-coherent.tar.gz esp32s31-port-4226-to-d468.bundle \
  esp32s31-apps-118d-to-59b0.bundle SHA256SUMS-progress325; do
  test ! -e "$task_name"
done
test "$(git -C "$task_nuttx" rev-parse --short=11 HEAD)" = d468c494ff8
test "$(git -C "$task_apps" rev-parse --short=9 HEAD)" = 59b05cd92
sha256sum -c build324-demo-irq-committed.sha256
mkdir checkpoint325
git -C "$task_nuttx" bundle create "$task_dir/esp32s31-port-4226-to-d468.bundle" \
  42267bfd79350f44ade398900e2bace34c263cc5..codex/esp32s31-port
git -C "$task_apps" bundle create "$task_dir/esp32s31-apps-118d-to-59b0.bundle" \
  118d5ff0e..codex/esp32s31-nettest
git -C "$task_nuttx" bundle verify "$task_dir/esp32s31-port-4226-to-d468.bundle"
git -C "$task_apps" bundle verify "$task_dir/esp32s31-apps-118d-to-59b0.bundle"
git -C "$task_nuttx" diff --binary > checkpoint325/nuttx-working.patch
git -C "$task_apps" diff --binary > checkpoint325/apps-working.patch
git -C "$task_nuttx" status --short > checkpoint325/nuttx-status.txt
git -C "$task_apps" status --short > checkpoint325/apps-status.txt
git -C "$task_nuttx" rev-parse HEAD > checkpoint325/nuttx-head.txt
git -C "$task_apps" rev-parse HEAD > checkpoint325/apps-head.txt
tar -czf checkpoint325/diagnostic-profiles.tar.gz -C "$task_nuttx" \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-cpu0 \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/production-regression \
  boards/risc-v/esp32s31/esp32s31-core-function-board/configs/wifi-flat
tar -czf firmware324-demo-coherent.tar.gz -C "$task_build" \
  nuttx nuttx.bin nuttx.map .config .config.prev include/nuttx/config.h \
  appfs.img bin bin_debug
mapfile -t task_logs < <(rg --files logs | \
  rg '/[a-z-]*(29[1-9]|3[01][0-9]|32[0-4])[^0-9]')
test "${#task_logs[@]}" -gt 25
tar -czf progress291-325-checkpoint.tar.gz README.md checkpoint325 \
  firmware313-demo-irq-route.tar.gz firmware318-demo-cpu1-init.tar.gz \
  firmware324-demo-coherent.tar.gz build29*.sha256 build30*.sha256 \
  build31*.sha256 build324-demo-irq-committed.sha256 \
  build-demo.sh flash-demo-pair.sh build-regression-wifi.sh \
  flash-regression-pair.sh network-probe.py network-repeat.py \
  buttons303-uart-lines.py save-progress325.sh \
  esp32s31-port-4226-to-d468.bundle esp32s31-apps-118d-to-59b0.bundle \
  "${task_logs[@]}"
sha256sum progress291-325-checkpoint.tar.gz firmware313-demo-irq-route.tar.gz \
  firmware318-demo-cpu1-init.tar.gz firmware324-demo-coherent.tar.gz \
  esp32s31-port-4226-to-d468.bundle esp32s31-apps-118d-to-59b0.bundle \
  > SHA256SUMS-progress325
sha256sum -c SHA256SUMS-progress325
printf 'PROGRESS_CHECKPOINT325=PASS\n'
