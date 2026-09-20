#!/bin/bash
set -euo pipefail
umask 077
cd /home/regex/work/esp32s31-openvela
record_dir=backups/2026-09-10-scan-stress
project_dir=diagnostics/idf-baseline
archive="$record_dir/idf-isolation170-183-checkpoint.tar.gz"
firmware="$record_dir/firmware180-restored-baseline.tar.gz"
bundle="$record_dir/esp32s31-port-6e102-to-66a0.bundle"
patch="$record_dir/s31-candidate180-diagnostics.patch"
checksum="$record_dir/SHA256SUMS-idf180"
for destination in "$archive" "$firmware" "$bundle" "$checksum"; do
  test ! -e "$destination"
done
set -x
test "$(git -C openvela-dev/nuttx rev-parse HEAD)" = 66a0aa00a3164d9c598cb11b4e67ed9e3ff8a393
if [ -e "$patch" ]; then
  git -C openvela-dev/nuttx diff --binary HEAD | cmp -s - "$patch"
else
  git -C openvela-dev/nuttx diff --binary HEAD > "$patch"
fi
git -C openvela-dev/nuttx bundle create "$PWD/$bundle" \
  6e1020517c29cb483c02faf731b09f945f9e5010..HEAD
git -C openvela-dev/nuttx bundle verify "$PWD/$bundle"
tar -czf "$firmware" -C openvela-dev/out/esp32s31-cmake-production6 \
  nuttx nuttx.bin .config
paths=("$project_dir/CMakeLists.txt" "$project_dir/README.md"
       "$project_dir/main" "$project_dir/partitions.csv"
       "$project_dir/sdkconfig.defaults" "$project_dir/sdkconfig"
       "$project_dir/sdkconfig-hal-phy" "$project_dir/sdkconfig-hal-wifi"
       "$project_dir/build-offline.sh" "$project_dir/backup-flash.sh"
       "$project_dir/flash-transaction.py" "$project_dir/run-comparison.py"
       "$project_dir/test-flash-safety.py" "$project_dir/summarize.py"
       "$record_dir/idf170-flash-backup" "$record_dir/README.md"
       "$record_dir/coexistence-candidate174-rejected.patch"
       "$record_dir/network-probe.py" "$record_dir/network-repeat.py"
       "$record_dir/tcp-nettest-client.py" "$record_dir/udp_probe_support.py"
       "$record_dir/verify-host-current.sh" "$record_dir/save-idf-checkpoint180.sh")
for build_name in build build-hal-phy build-hal-wifi; do
  for artifact in s31_wifi_baseline.elf s31_wifi_baseline.bin s31_wifi_baseline.map \
                  flasher_args.json bootloader/bootloader.bin \
                  partition_table/partition-table.bin; do
    paths+=("$project_dir/$build_name/$artifact")
  done
done
for log in "$record_dir"/logs/idf17* "$record_dir"/logs/idf-baseline17* \
           "$record_dir"/logs/coexistence-*174* "$record_dir"/logs/network169* \
           "$record_dir"/logs/network175* "$record_dir"/logs/network177* \
           "$record_dir"/logs/network179* "$record_dir"/logs/build17[48].sha256 \
           "$record_dir"/logs/build180.sha256 "$record_dir"/logs/flash17[48]* \
           "$record_dir"/logs/flash180* "$record_dir"/logs/production-cmake17[48]* \
           "$record_dir"/logs/production-cmake180* "$record_dir"/logs/production-make176* \
           "$record_dir"/logs/production-make182* "$record_dir"/logs/production-smoke181* \
           "$record_dir"/logs/host178* "$record_dir"/logs/host183*; do
  test -f "$log"
  paths+=("$log")
done
tar -czf "$archive" "${paths[@]}"
sha256sum "$archive" "$firmware" "$bundle" "$patch" \
  "$record_dir/coexistence-candidate174-rejected.patch" > "$checksum"
sha256sum --check "$checksum"
echo IDF_CHECKPOINT=PASS
