# BLE/Wi-Fi coexistence: bounded source audit

Read-only audit while864 continues. No Kconfig/source change, build, radio or
UART operation. Original cases4.1.105/4.1.104 remain unimplemented/unverified.

- ESP32S31_BLE currently depends on BUILD_FLAT, !SMP, !ESPRESSIF_WIFI and
  !ESPRESSIF_SPIRAM_USER_HEAP. The939 profile is deliberately BLE-only.
- The sdkconfig shim enables CONFIG_SW_COEXIST_ENABLE when Wi-Fi is selected.
  Wi-Fi already links libcoexist and the NuttX common coex OS adapter.
- Actual939 includes the pinned controller/esp32s31/bt.c and
  porting_btdm/controller/btdm_common/src/btdm_coex.c. Do not infer that the
  older commented coex block in local esp32s31_ble_controller.c is the only
  integration point. btdm_coex_init/enable/disable call the shared coex library
  once Wi-Fi activates CONFIG_SW_COEXIST_ENABLE.
- In the locked btdm_coex.c, wr_btdm_coex_version_get returns0 in the enabled
  branch while the code assigning its output pointers is inside #if0. The
  pinned comment says it awaits a coexist submodule update. This is a concrete
  success-without-output path to resolve before trusting combined behavior;
  no claim is made that it has already caused a target failure.
- Combined operation also needs shared PHY/coex lifecycle and IRQ/cache-off
  closure review. Wi-Fi and BT both invoke coex lifecycle functions; the
  closed library's behavior must be verified, not presumed from link success.
- Bluetooth and Wi-Fi CMake lists overlap PHY/security sources. Preserve the
  separately working profiles while consolidating any future combined build.

Published4.1.105 specifies100 disconnect/reconnect rounds with reachability
checks;4.1.104 specifies100 directed Wi-Fi scans. Their BLE scan/advertising
background must be real. Wi-Fi-only loops are not coexistence acceptance.

Under the current xTS-first deadline, first qualify independent BLE939 and
network904/911 on board. This audit adds no PASS, no test count and no claimed
coexistence feature. Original references and the running864 are unchanged.
