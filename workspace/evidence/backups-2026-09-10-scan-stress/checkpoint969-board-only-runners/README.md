# Board-only original xTS execution preparation969

HOST PREPARATION ONLY. No firmware build, flash, UART access, Wi-Fi association,
DHCP/NTP, Bluetooth command or board test occurred during this preparation.
The clock longrun remains unassociated and owns the board until actual24h.

- Wi-Fi: separate strict WAPI AP rows from diagnostics so SSIDs containing
  error words cannot cause false failures. Original100 iterations, visible AP
  and list-change requirements remain; case result and cleanup are separate.
- BLE: original enable/state/disable/state, callbacks2/0 and explicit states.
  Uses existing939 candidate, fresh RAM database, no peer or extra iterations.
- KVDB: existing948 candidate and already mounted isolated scratch LittleFS.
  Runs original30-case suite once and checks each RUN/OK plus summary. Reuses
  existing raw tty without requested reset/modem-line changes. No formatting.
- Crypto/watchdog: reject occupied UART before serial open/reset. Algorithms,
  original four watchdog modes and acceptance thresholds are unchanged.
- ROMFS/FATUTF8: source/configuration review found no necessary change to892.

Validation: Python syntax/help, focused host Wi-Fi row parsing and KV mount
transcript parsing; these are host checks, not xTS hardware results.
Use current detailed target-sequence documents. Longrun completion remains a
prerequisite; fuser alone does not prove longrun acceptance.
