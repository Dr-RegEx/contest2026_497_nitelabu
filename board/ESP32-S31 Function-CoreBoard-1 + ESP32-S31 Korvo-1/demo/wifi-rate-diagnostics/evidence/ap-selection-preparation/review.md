# ESP32-S31 initial AP selection candidate — offline preparation

This candidate changes unconstrained station connection requests from first-matching-SSID scanning to all-channel scanning with RSSI sorting. It does not establish that the strongest RSSI AP has the best end-to-end path to the test peer. It does not add automatic roaming, periodic scans, or AP switching while connected.

## Existing contract and minimal change

The station cache is zero initialized, but `WIFI_ADPT_EVT_STA_START` replaces it using `esp_wifi_get_config()`. A static default initializer would therefore be insufficient. SSID/password/BSSID/frequency/security setters copy the cached configuration. The connection routine installs that configuration immediately before `esp_wifi_connect()`.

The change is confined to `esp_wifi_sta_connect()` in `openvela-dev/nuttx/arch/risc-v/src/esp32c6/esp_wifi_adapter.c`, guarded by `CONFIG_ARCH_CHIP_ESP32S31`. While holding the existing Wi-Fi mutex, it copies the cache locally and applies:

| Request | Scan policy | Sorting |
|---|---|---|
| No explicit BSSID and channel zero | `WIFI_ALL_CHANNEL_SCAN` | `WIFI_CONNECT_AP_BY_SIGNAL` |
| Explicit BSSID or nonzero channel | `WIFI_FAST_SCAN` | Existing value retained |

Only the local copy is passed to the vendor. Cached configuration, SSID/password, BSSID, channel, security thresholds, PMF, SAE, and other fields are not rewritten by this policy. Explicit preferences restore fast-scan semantics even if a previous vendor configuration was all-channel. The non-S31 branch retains its original call.

The pinned HAL documents `channel` as a **scan-start hint**, not a strict channel lock. Preserving an explicit frequency request means retaining that hint and the existing fast-scan behavior; this candidate makes no regulatory/channel-lock guarantee. A pinned BSSID remains the requested AP. There is no automatic cross-SSID selection.

The existing disconnect handler continues its existing reconnect path using the vendor's installed configuration. This change does not introduce a new roaming mechanism or new reconnect loop.

## Evidence and reproduction

- Original adapter source immediately before this candidate: [esp_wifi_adapter.before.c](esp_wifi_adapter.before.c).
- Exact incremental delta, excluding existing RX diagnostics: [ap-selection.patch](ap-selection.patch).
- Reproducible host test: `python3 tools/tests/wifi-queue-ownership/ap_selection.py` from the workspace root.
- Actual successful execution: [host-test.log](host-test.log), including source/policy hashes and compiler command.

The host test extracts the actual production policy statements and the scan/sort enum declarations from the pinned HAL. A populated configuration fixture checks 48 combinations of explicit BSSID, channel hints, prior scan policy, and vendor success/failure. Full-byte comparisons verify that the cache is unchanged and all installed configuration bytes other than the intended policy fields are retained. Strict compiler warnings, ASan, and UBSan pass. The fixture is not the complete target ABI and does not validate radio selection or throughput.

`git diff --check` and reverse application of the exact patch pass. No firmware build, serial interaction, flash, board configuration, or board test was performed for this preparation. Target compilation and real AP selection/performance remain to be evaluated by the root agent with a distinct paired candidate.
