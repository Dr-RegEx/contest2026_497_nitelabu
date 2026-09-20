Wapi information candidate1430, target NOTRUN.

Evidence: original1311 show printed AP ff:ff:ff:ff:ff:ff while connected and logged unsupported PTA ioctl35815. S31 bssid GET returned configured wildcard, not active AP. Now only S31 connected GET uses esp_wifi_sta_get_ap_info and propagates its failure. Disconnected behavior and setter unchanged. wapi save_config also uses this GET: saved profile will capture real AP BSSID as its existing API intends; validate save/reconnect after flashing, preserve old profile first.

ESP wlan explicitly returns EOPNOTSUPP for optional PTA GET. Wapi suppresses only that expected unsupported diagnostic, prints not supported in show; all other errors still propagate/log. No priority value invented and no setter/coexistence capability added. The original required show fields remain available. Candidate includes1427SCP initialization error propagation; config intended identical1413. Build in progress, no UART/board changes during1425.

Next validation after current100rounds: keep old1413 results; candidate build/pair verification; flash only when UART released, original show/sense plus bounded saved-profile/reconnect regression because BSSID GET changed. Do not overwrite sole known-good saved profile for this regression; use separate file and restore original selection.
