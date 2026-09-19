#!/bin/bash
# Build the basic demo offline, preserving the source-tree Make configuration.
set -euo pipefail
task_root=/home/regex/work/esp32s31-openvela
task_build=$task_root/openvela-dev/out/esp32s31-cmake-demo
task_receipt=${1:?Pass a new absolute build receipt}
task_profile=${S31_DEMO_PROFILE:-demo}
case "$task_profile" in
  demo|demo-rmt-netapps-competition-offline|demo-rmt-netapps-competition|demo-rmt-netapps-iperf|demo-rmt-netapps|demo-rmt-netapps-ssh|demo-cpu1-init|demo-i2c|demo-i2c-cpu1-init|demo-rmt|demo-rmt-event|demo-rmt-mqueue|demo-rmt-xts-core|demo-rmt-xts-common|demo-rmt-xts-memory|demo-rmt-xts-cxx|demo-rmt-xts-reboot|demo-rmt-xts-ostest|demo-rmt-he|demo-rmt-tcpdiag|demo-rmt-arp-learn|demo-rmt-arp-learn-he|demo-rmt-arp-queue) ;;
  demo-rmt-xts-io|demo-rmt-xts-drivers|demo-rmt-xts-rtc|demo-rmt-xts-rng|demo-rmt-xts-crypto|demo-rmt-xts-fsheap|demo-rmt-xts-ymodem|demo-rmt-xts-aes|demo-rmt-xts-aes-modes|demo-rmt-xts-sha|demo-rmt-xts-hmac|demo-rmt-xts-ecdsa|demo-rmt-xts-ecc|demo-rmt-xts-ecc-offline|demo-rmt-xts-ecc-offline-log|demo-rmt-xts-standby) ;;
  *) exit 2 ;;
esac
if [[ "$task_profile" = demo-rmt-netapps-competition-offline ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-competition-offline
elif [[ "$task_profile" = demo-rmt-netapps-competition ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-competition
elif [[ "$task_profile" = demo-rmt-netapps-iperf ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-netapps-iperf
elif [[ "$task_profile" = demo-rmt-netapps-ssh ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-netapps-ssh
elif [[ "$task_profile" = demo-rmt-netapps* ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-netapps
elif [[ "$task_profile" = demo-rmt-xts-standby ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-standby
elif [[ "$task_profile" = demo-rmt-xts-ecc-offline-log ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-ecc-offline-log
elif [[ "$task_profile" = demo-rmt-xts-ecc-offline ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-ecc-offline
elif [[ "$task_profile" = demo-rmt-xts-ecc* ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-ecc
elif [[ "$task_profile" = demo-rmt-xts-ecdsa ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-ecdsa
elif [[ "$task_profile" = demo-rmt-xts-hmac ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-hmac
elif [[ "$task_profile" = demo-rmt-xts-sha ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-sha
elif [[ "$task_profile" = demo-rmt-xts-aes-modes ]]; then
  task_build=$task_root/openvela-dev/out/esp32s31-xts-aes-modes
fi
task_build=$task_root/openvela-dev/out/esp32s31-network1332
test ! -e "$task_receipt"
test -d "$task_root/openvela-dev/apps/crypto/mbedtls/mbedtls"
if [[ "$task_profile" = demo-rmt-xts-* ]]; then
  test -f "$task_root/openvela-dev/apps/testing/cmocka/cmocka/src/cmocka.c"
fi
if [[ "$task_profile" = demo-rmt-xts-cxx ]]; then
  test -f "$task_root/openvela-dev/nuttx/libs/libxx/libcxx/libcxx/include/__config"
  test -f "$task_root/openvela-dev/nuttx/libs/libxx/libcxxabi/libcxxabi/src/cxa_guard.cpp"
fi
if [[ "$task_profile" = demo-rmt-xts-rng || "$task_profile" = demo-rmt-xts-crypto || "$task_profile" = demo-rmt-xts-fsheap || "$task_profile" = demo-rmt-xts-aes || "$task_profile" = demo-rmt-xts-aes-modes || "$task_profile" = demo-rmt-xts-sha || "$task_profile" = demo-rmt-xts-hmac || "$task_profile" = demo-rmt-xts-ecdsa || "$task_profile" = demo-rmt-xts-ecc* ]]; then
  test -f "$task_root/openvela-dev/apps/testing/drivers/nist-sts/sts/src/assess.c"
fi
cd "$task_root/openvela-dev/nuttx"
task_saved=$(mktemp -d /tmp/s31-demo-source-config.XXXXXX)
restore_config() {
  if [ -f "$task_saved/.config" ]; then mv "$task_saved/.config" .config; fi
  if [ -f "$task_saved/config.h" ]; then mv "$task_saved/config.h" include/nuttx/config.h; fi
  rmdir "$task_saved"
}
trap restore_config EXIT
if [ -f .config ]; then mv .config "$task_saved/.config"; fi
if [ -f include/nuttx/config.h ]; then mv include/nuttx/config.h "$task_saved/config.h"; fi
. "$task_root/s31-reference/tmp/esp-idf-clean/export.sh" > "$task_root/backups/2026-09-10-scan-stress/logs/demo-idf-export.log" 2>&1
export PATH="$task_root/backups/2026-09-10-scan-stress/offline-bin:$task_root/s31-reference/.venv-nuttx/bin:$PATH"
export ESP_HAL_3RDPARTY_LOCAL="$task_root/s31-reference/deps/esp-hal-3rdparty"
set -x
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:"$task_profile" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$task_build" --target resetconfig
# resetconfig removes the configure-time header. Regenerate explicitly:
# Ninja can otherwise reject the missing header before rerunning CMake.
cmake -S . -B "$task_build" -G Ninja \
  -DBOARD_CONFIG=esp32s31-core-function-board:"$task_profile" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
if [[ "$task_profile" = *-he ]]; then
  rg -q '^CONFIG_ESPRESSIF_WIFI_STA_11AX=y$' "$task_build/.config"
else
  rg -q '^# CONFIG_ESPRESSIF_WIFI_STA_11AX is not set$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-tcpdiag || "$task_profile" = demo-rmt-arp-* ]]; then
  rg -q '^CONFIG_NET_STATISTICS=y$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-arp-learn* ]]; then
  rg -q '^CONFIG_NET_ARP_IPIN=y$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-arp-queue ]]; then
  rg -q '^CONFIG_NET_ARP_SEND=y$' "$task_build/.config"
  rg -q '^CONFIG_NET_ARP_SEND_QUEUE=y$' "$task_build/.config"
  rg -q '^# CONFIG_NET_ARP_IPIN is not set$' "$task_build/.config"
fi
if [[ "$task_profile" = *cpu1-init ]]; then
  rg -q '^CONFIG_SMP_DEFAULT_CPUSET=0x2$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-mqueue ]]; then
  rg -q '^CONFIG_ESP32S31_MQUEUE_TEST=y$' "$task_build/.config"
else
  rg -q '^# CONFIG_ESP32S31_MQUEUE_TEST is not set$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-event ]]; then
  rg -q '^CONFIG_ESP32S31_WIFI_EVENT_TEST=y$' "$task_build/.config"
else
  rg -q '^# CONFIG_ESP32S31_WIFI_EVENT_TEST is not set$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt* ]]; then
  for task_option in RMT ESP_RMT WS2812 WS2812_NON_SPI_DRIVER EXAMPLES_S31LED; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q '^CONFIG_WS2812_LED_COUNT=1$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-i2c* || "$task_profile" = demo-rmt* ]]; then
  rg -q '^CONFIG_ESPRESSIF_I2C0=y$' "$task_build/.config"
  rg -q '^CONFIG_ESPRESSIF_I2C0_SCLPIN=50$' "$task_build/.config"
  rg -q '^CONFIG_ESPRESSIF_I2C0_SDAPIN=51$' "$task_build/.config"
  rg -q '^CONFIG_SYSTEM_I2CTOOL=y$' "$task_build/.config"
fi
if rg -q '^CONFIG_ARCH_BUTTONS=y$' "$task_build/.config"; then
  for task_option in ARCH_IRQBUTTONS ESPRESSIF_GPIO_IRQ INPUT INPUT_BUTTONS INPUT_BUTTONS_LOWER EXAMPLES_BUTTONS; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
fi
if [[ "$task_profile" = demo-rmt-xts-* ]]; then
  for task_option in TESTING_CMOCKA TESTS_TESTSUITES CM_MM_TEST CM_SCHED_TEST CM_SYSCALL_TEST; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
fi
if [[ "$task_profile" = demo-rmt-xts-ecc-offline* ]]; then
  rg -q '^CONFIG_NETINIT_NETLOCAL=y$' "$task_build/.config"
  ! rg -q '^CONFIG_NETINIT_DHCPC=y$' "$task_build/.config"
fi
cmake --build "$task_build" -j8
python3 "$task_root/backups/2026-09-10-scan-stress/verify-wifi-artifact.py" "$task_build"
test -f "$task_build/bin/s31demo"
test -f "$task_build/appfs-root/s31demo"
test -f "$task_build/appfs-root/s31demo.sha256"
if [[ "$task_profile" = demo-rmt-xts-* ]]; then
  for task_app in cmocka_mm_test cmocka_sched_test cmocka_syscall_test; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
if [[ "$task_profile" = demo-rmt-xts-ostest ]]; then
  rg -q '^CONFIG_TESTING_OSTEST=y$' "$task_build/.config"
  test -f "$task_build/appfs-root/ostest"
fi
if [[ "$task_profile" = demo-rmt-xts-ymodem ]]; then
  rg -q '^CONFIG_SYSTEM_YMODEM=y$' "$task_build/.config"
  for task_app in sb rb; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
if [[ "$task_profile" = demo-rmt-xts-common || "$task_profile" = demo-rmt-xts-memory || "$task_profile" = demo-rmt-xts-cxx || "$task_profile" = demo-rmt-xts-reboot ]]; then
  for task_option in NET_LOCAL FS_FAT FAT_LFN TESTING_GETPRIME TESTING_SCANFTEST EXAMPLES_HELLO; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q '^CONFIG_IOB_NBUFFERS=128$' "$task_build/.config"
  rg -q '^CONFIG_IOB_NCHAINS=4$' "$task_build/.config"
  rg -q '^CONFIG_NAME_MAX=255$' "$task_build/.config"
  for task_app in getprime scanftest hello; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
if [[ "$task_profile" = demo-rmt-xts-memory || "$task_profile" = demo-rmt-xts-cxx || "$task_profile" = demo-rmt-xts-reboot ]]; then
  for task_app in fstest ramtest; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
if [[ "$task_profile" = demo-rmt-xts-crypto || "$task_profile" = demo-rmt-xts-aes || "$task_profile" = demo-rmt-xts-aes-modes || "$task_profile" = demo-rmt-xts-sha || "$task_profile" = demo-rmt-xts-hmac || "$task_profile" = demo-rmt-xts-ecdsa || "$task_profile" = demo-rmt-xts-ecc* ]]; then
  for task_option in CRYPTO_CRYPTODEV CRYPTO_CRYPTODEV_SOFTWARE_CRYPTO TESTING_CRYPTO; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_app in cmocka_des3cbc cmocka_aescbc cmocka_aesctr cmocka_aesxts cmocka_hmac cmocka_hash cmocka_crc32 cmocka_ecdsa; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
if [[ "$task_profile" = demo-rmt-xts-aes || "$task_profile" = demo-rmt-xts-aes-modes || "$task_profile" = demo-rmt-xts-sha || "$task_profile" = demo-rmt-xts-hmac || "$task_profile" = demo-rmt-xts-ecdsa || "$task_profile" = demo-rmt-xts-ecc* ]]; then
  rg -q '^CONFIG_ESP32S31_CRYPTO_AES_CBC=y$' "$task_build/.config"
  rg -q '^CONFIG_CRYPTO_CRYPTODEV_HARDWARE=y$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-xts-aes-modes || "$task_profile" = demo-rmt-xts-sha || "$task_profile" = demo-rmt-xts-hmac || "$task_profile" = demo-rmt-xts-ecdsa || "$task_profile" = demo-rmt-xts-ecc* ]]; then
  rg -q '^CONFIG_ESP32S31_CRYPTO_AES_MODES=y$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-xts-sha || "$task_profile" = demo-rmt-xts-hmac || "$task_profile" = demo-rmt-xts-ecdsa || "$task_profile" = demo-rmt-xts-ecc* ]]; then
  rg -q '^CONFIG_ESP32S31_CRYPTO_SHA=y$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-xts-hmac || "$task_profile" = demo-rmt-xts-ecdsa || "$task_profile" = demo-rmt-xts-ecc* ]]; then
  rg -q '^CONFIG_ESP32S31_CRYPTO_HMAC=y$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-xts-ecdsa || "$task_profile" = demo-rmt-xts-ecc* ]]; then
  rg -q '^CONFIG_ESP32S31_CRYPTO_ECDSA_VERIFY=y$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-xts-ecc* ]]; then
  rg -q '^CONFIG_ESP32S31_CRYPTO_ECC_POINT_MULT=y$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-xts-standby ]]; then
  for task_option in MM_KASAN MM_KASAN_GENERIC MM_KASAN_INSTRUMENT_ALL SYSTEM_RESMONITOR; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q '^CONFIG_FS_HEAPSIZE=0$' "$task_build/.config"
  test -f "$task_build/appfs-root/showinfo"
fi
if [[ "$task_profile" = demo-rmt-xts-rng || "$task_profile" = demo-rmt-xts-crypto || "$task_profile" = demo-rmt-xts-fsheap || "$task_profile" = demo-rmt-xts-aes || "$task_profile" = demo-rmt-xts-aes-modes || "$task_profile" = demo-rmt-xts-sha || "$task_profile" = demo-rmt-xts-hmac || "$task_profile" = demo-rmt-xts-ecdsa || "$task_profile" = demo-rmt-xts-ecc* ]]; then
  rg -q '^CONFIG_TESTING_NIST_STS=y$' "$task_build/.config"
  test -f "$task_build/appfs-root/nist_sts"
  cmp "$task_build/appfs-root/nist-template9" "$task_root/openvela-dev/apps/testing/drivers/nist-sts/sts/templates/template9"
fi
if [[ "$task_profile" = demo-rmt-xts-rtc ]]; then
  for task_option in RTC_ALARM RTC_PERIODIC RTC_IOCTL SIG_EVTHREAD; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  test -f "$task_build/appfs-root/cmocka_driver_rtc"
fi
if [[ "$task_profile" = demo-rmt-xts-drivers || "$task_profile" = demo-rmt-xts-rtc ]]; then
  for task_option in TESTING_DRIVER_TEST TIMER; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_app in cmocka_driver_timer cmocka_driver_uart; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
if [[ "$task_profile" = demo-rmt-xts-io || "$task_profile" = demo-rmt-xts-drivers ]]; then
  for task_option in EXAMPLES_POPEN EXAMPLES_PIPE TESTS_TESTCASES FS_TEST FS_TEST_EDONLY SYSTEM_TASKSET; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_app in popen pipe md5_test taskset sh; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
if [[ "$task_profile" = demo-rmt-xts-reboot || "$task_profile" = demo-rmt-xts-io ]]; then
  rg -q '^CONFIG_BOARDCTL_RESET=y$' "$task_build/.config"
  rg -q '^# CONFIG_NSH_DISABLE_REBOOT is not set$' "$task_build/.config"
fi
if [[ "$task_profile" = demo-rmt-xts-cxx ]]; then
  for task_option in HAVE_CXX LIBCXX LIBCXXABI CXX_RTTI CXX_EXCEPTION; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_app in helloxx cxxtest; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
if [[ "$task_profile" = demo-rmt* ]]; then
  test -f "$task_build/appfs-root/s31led"
  test -f "$task_build/appfs-root/s31led.sha256"
fi
if [[ "$task_profile" = demo-i2c* || "$task_profile" = demo-rmt* ]]; then
  test -f "$task_build/appfs-root/i2c"
fi
if rg -q '^CONFIG_ARCH_BUTTONS=y$' "$task_build/.config"; then
  test -f "$task_build/appfs-root/buttons"
fi
if [[ "$task_profile" = demo-rmt-xts-fsheap ]]; then
  rg -q '^CONFIG_FS_HEAPSIZE=4194304$' "$task_build/.config"
  rg -q '^CONFIG_FS_HEAPBUF_SECTION=".s31.fsheap"$' "$task_build/.config"
  test -f "$task_build/appfs-root/fstest"
fi
test "$(stat -c %s "$task_build/nuttx.bin")" -le 2088960
if [[ "$task_profile" = demo-rmt-netapps* ]]; then
  for task_option in LIB_CURL UTILS_CURL NETUTILS_FTPD EXAMPLES_FTPD; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  for task_app in curl ftpd_start wapi; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
test "$(stat -c %s "$task_build/appfs.img")" -le 3145728
(if [[ "$task_profile" = demo-rmt-netapps-ssh ]]; then
  rg -q '^CONFIG_LIB_SSH=y$' "$task_build/.config"
  rg -q '^CONFIG_UTILS_SSH=y$' "$task_build/.config"
  test -f "$task_build/appfs-root/scp"
fi
if [[ "$task_profile" = demo-rmt-netapps-iperf ]]; then
  rg -q '^CONFIG_UTILS_IPERF2=y$' "$task_build/.config"
  test -f "$task_build/appfs-root/iperf2"
  test -f "$task_build/appfs-root/scp"
fi
if [[ "$task_profile" = demo-rmt-netapps-competition* ]]; then
  for task_option in ESP32S31_CRYPTO_AES_CBC ESP32S31_CRYPTO_AES_MODES ESP32S31_CRYPTO_SHA ESP32S31_CRYPTO_HMAC ESP32S31_CRYPTO_ECDSA_VERIFY ESP32S31_CRYPTO_ECC_POINT_MULT NETUTILS_CJSON WIRELESS_WAPI_INITCONF; do
    rg -q "^CONFIG_${task_option}=y$" "$task_build/.config"
  done
  rg -q '^CONFIG_WIRELESS_WAPI_CONFIG_PATH="/apps/s31-wapi-xts.conf"$' "$task_build/.config"
  rg -q '^CONFIG_NETINIT_NETLOCAL=y$' "$task_build/.config"
  test -f "$task_build/appfs-root/wapi"
  for task_app in scp iperf2 s31demo s31led; do
    test -f "$task_build/appfs-root/$task_app"
  done
fi
set -o noclobber; sha256sum "$task_build/nuttx.bin" "$task_build/appfs.img" > "$task_receipt")
