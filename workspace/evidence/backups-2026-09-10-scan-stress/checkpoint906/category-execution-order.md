# Category and common execution order after longrun864

Board identity: ESP32-S31 Function-CoreBoard-1, as confirmed by the user.
September19 submission; reserve September17 physical fixtures and September18
fixes/demo. Do not interrupt864's original24-hour run merely to run shorter
cases. Planned completion September16 16:43 +08:00, subject to actual result.

1. Save864 final UART/JSON/receipt and report standby/time separately.
2. Backup the fixed1MiB scratch Flash twice using backup-flash-scratch.py.
   Run865 original raw Flash block case before any filesystem formatting.
3. Run868 original watchdog modes0/1/2/3, then883 original eight crypto apps
   and hardware-path evidence. Failed candidates retain their failures and
   diagnostics; known810/860 images are archived for fallback.
4. Run892 ROMFS and FATUTF8 original cases using xts-category-fs.py. These use
   disposable RAM storage, no Flash-formatting prerequisite. Then prioritize
   resource-bounded original filesystem cases from category-storage-audit.md.
   Do not infer persistent/power-loss acceptance from RAM tests.
5. Prepare isolated LittleFS /data on895's /dev/xtsflash only after step2.
   Run original30-case KVDB suite and stability10 workload. Preserve manifests
   and raw outputs; production/apps never serves as disposable storage.
6. Run891 CountryCode original US/CN sequence,100ifup/down and100unassociated
   scans using xts-wifi-lifecycle.py. Then authorized private WPA2 association
   and published curl HTTP page/file plus FTP PC get/put, with exact file-size
   and transfer evidence. Never print or put Wi-Fi passwords on shell CLI.
7. September17 physical session: GPIO886 jumper, BMI160887100reads, BMI160896
   original uORB30listener invocations (three rates times ten rounds), RESET
   button and ten genuine cold starts. PWM waveform capture when its candidate
   is available. Sensor896 reuses887 wiring.
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
