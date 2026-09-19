# BLE-only GATT build compatibility

The 1077 profile did not enable the GATT server. Test 1512 therefore stopped at the unsupported gatts command; no PC GATT connection was attempted.

1513 enables BLUETOOTH_GATT_SERVER and BT_GATT_DYNAMIC_DB, but exposed unconditional ATT-over-BR references in the framework SAL. The locked Zblue att.h exports these APIs only with CONFIG_BT_ATT_OVER_BR.

1515 guards SDP registration, BR connection callbacks and BR connection/disconnection paths with that same option. BLE database registration and callbacks stay enabled. Unsupported BR service requests are rejected before modifying the shared attribute database. The first 1515 build found one remaining disconnect reference; build1515b includes that correction. No classic Bluetooth controller feature is enabled.

This is an isolated FLAT BLE image. Compilation and GATT diagnostics do not constitute xTS interval acceptance or SMP/MMU/Wi-Fi coexistence acceptance. The selected sprint count remains 70/88.
