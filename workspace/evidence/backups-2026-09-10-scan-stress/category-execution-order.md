# Category and common execution order after longrun864

Board identity: ESP32-S31 Function-CoreBoard-1, as confirmed by the user.
September19 submission; reserve September17 physical fixtures and September18
fixes/demo. Do not interrupt864's original24-hour run merely to run shorter
cases. The initial estimate was September16 16:43 +08:00. Host REALTIME and MONOTONIC have diverged; audit actual duration and time basis before accepting automatic results (clock864-host-audit.md).

1. Save864 final UART/JSON/receipt and report standby/time separately.
2. Backup the fixed1MiB scratch Flash twice using backup-flash-scratch.py.
   Run946 original raw Flash block case (replaces865 with PSRAM-stack dispatch) before any filesystem formatting.
   Then the rebuilt raw Flash candidate947 for original raw dd throughput on /dev/xtsraw before storing FS evidence.
3. Run868 original watchdog modes0/1/2/3, then883 original eight crypto apps
   and hardware-path evidence (`xts-crypto.py --backend hardware-ecc`;
   the default software mode does not enforce883 hardware coverage).
   Failed candidates retain their failures and
   diagnostics; known810/860 images are archived for fallback.
4. Run892 ROMFS and FATUTF8 original cases using xts-category-fs.py. These use
   disposable RAM storage, no Flash-formatting prerequisite. Then prioritize
   resource-bounded original filesystem cases from category-storage-audit.md and fs-short-target-sequence.md. The one-case runner is ready for5.1.5/6/8/9/10 on976 (950 archived);5.1.3 uses the explicitly disclosed internal-SRAM candidate974 with unchanged100 rounds.
   Do not infer persistent/power-loss acceptance from RAM tests.
5. Prepare isolated LittleFS /data on948's /dev/xtsflash only after step2.
   Run original30-case KVDB suite and stability10 workload. Preserve manifests
   and raw outputs; production/apps never serves as disposable storage.
   For software recovery cases use973 and fs-recovery-target-sequence.md:
   test03 reboot/crash needs both stages; test04 has an archived necessary
   checker fix and must be reported as patched test source. No forceformat
   between reset and verification. Reserve enough free capacity and keep
   separate test directories; do not run concurrently with kvdbd.
6. Use combined967 competition/network image (904 lacks save_config/reconnect) for CountryCode original US/CN sequence,100ifup/down and100unassociated
   scans using xts-wifi-lifecycle.py. Association-dependent cases are currently deferred: the user explicitly
   instructed not to provision Wi-Fi on September16. Do not run WPA2 association,
   DHCP, NTP, curl/FTP/SCP/iperf network traffic until that restriction is lifted.
   Offline preparation may continue. Never print or put Wi-Fi passwords on shell CLI.
7. September17 physical session: GPIO886 jumper, BMI160887100reads, BMI160896
   original uORB30listener invocations (three rates times ten rounds), RESET
   button and ten genuine cold starts. PWM898 waveform capture. Sensor896 reuses887 wiring.
8. Restore a board-verified SMP/MMU demo pair, then record live HTTP/RGB/button/
   storage functionality and actual xTS outcomes. Candidate builds do not count
   as hardware success. Leave explicit audio/USB/Bluetooth and any other gaps.

Every completed original case gets an immediate user update and ledger entry.
Single board means no concurrent serial clients, flashes, hardware tests or
reboots. Offline builds/source review may proceed concurrently with longrun.
Actual timings of unrun workloads are unknown; monitor runtime rather than
claiming the whole category suite fits a predetermined short slot. Original
fstest1000 can be long; do not start it if it would displace the reserved
physical session without reporting the tradeoff to the user first.

New offline candidates: audio968 (adds original44.1kHz;963 frozen), BLE/bttool939, USB/ADB965. All remain BUILD ONLY. Schedule board audio and BLE enable/disable after common tests; peer BLE cases and native J4 USB connection require the physical session. See audio, ble and usb-adb target sequence/checkpoint documents. USB source VBUS isolation must be verified before connection.

Host network tools961/962 and actual SCP/iperf commands are in
host-network-target-sequence.md. Host-only loopback results are not board PASS.

967 supersedes911 for the unified demo and adds original WAPI persistence/reconnect commands. Run4.1.28/30/29 per host-network-target-sequence.md after authenticated networking; all remain TARGET NOT RUN.
