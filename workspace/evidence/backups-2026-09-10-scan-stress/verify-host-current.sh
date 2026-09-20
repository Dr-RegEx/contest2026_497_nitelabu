#!/bin/bash
set -euo pipefail
set -x
cd /home/regex/work/esp32s31-openvela/openvela-dev/nuttx
for test in test_elf_default_heapsize test_elf_unnamed_symbols test_esp_hr_timer_lifecycle test_esp32s31_wifi_task test_esp32s31_irq_completion test_esp32s31_irq_unmap test_esp_wifi_scan_results test_esp_wifi_connect test_procfs_meminfo_lifecycle test_esp32s31_phy_clock test_esp32s31_dhcp_probe; do
  python3 "tools/$test.py"
done
for test in test_esp32s31_wifi_tx_budget test_esp_wifi_rx_queue test_esp_wifi_ba_window test_esp32s31_wifi_got_ip test_esp32s31_wifi_modem_status test_esp32s31_ping_probe; do
  python3 "tools/$test.py"
done
if [ -n "${S31_PROTOCOL_TEST_REVISION:-}" ]; then
  echo "Protocol unit source revision: $S31_PROTOCOL_TEST_REVISION (temporary frame statistics are not covered)"
  python3 tools/test_esp32s31_wifi_protocol.py --revision "$S31_PROTOCOL_TEST_REVISION"
else
  python3 tools/test_esp32s31_wifi_protocol.py
fi
python3 tools/test_esp32s31_wireless_make.py --hal /home/regex/work/esp32s31-openvela/s31-reference/deps/esp-hal-3rdparty
tools/nxstyle -r 707,23 libs/libc/elf/elf_symbols.c
tools/nxstyle -r 750,4 -r 772,17 -r 834,10 fs/procfs/fs_procfsmeminfo.c
git diff --check
cd /home/regex/work/esp32s31-openvela/s31-reference
python3 platform/tools/verify_f0_dependencies.py
