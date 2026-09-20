# 1727 BLE Audio/ISO build audit

- Date: 2026-09-18
- Profile: `xts-flat-ble-audio-mesh`
- Requested scope: ZBlue LE Audio/ISO roles, BAP unicast/broadcast roles, and
  the Bluetooth Mesh configuration.

The first candidate did not produce `nuttx.bin` or a receipt. Compilation
reached the existing ZBlue ISO implementation and stopped on API mismatches
between `subsys/bluetooth/host/iso.c` and the checked-in ISO headers.
Representative errors were:

- `struct bt_iso_chan` has no member `conn`;
- `hci_le_cis_established` has conflicting declaration/definition signatures;
- `hdev` is undeclared in the ISO path;
- `cig_init_cis`, `hci_le_remove_cig`, `iso_chans_connecting`, and
  `get_free_big` are called with argument counts that do not match their
  declarations.

## 1727 compatibility attempt

A bounded source compatibility patch was applied to
`external/zblue/zblue/subsys/bluetooth/host/iso.c`: the seven stale calls now
use the existing `hdev` context (`chan->iso->hdev`, `cig->hdev`,
`padv->hdev`) and the CIS event handler signature matches
`iso_internal.h`. The ZBlue/NuttX atomic bridge was also made safe for audio
headers that include `atomic_types.h` before `atomic.h` by sharing one set of
NuttX-backed aliases in `zblue/include/zephyr/sys/atomic{,_types}.h`.

With those changes, the full 1796-target build completed and generated
`openvela-dev/out/esp32s31-xts-flat-ble-audio-mesh-1727/nuttx.bin` (1,304,228
bytes). The build wrapper was corrected to check the symbols this ZBlue
revision actually resolves (`CONFIG_BT_BAP_UNICAST`, client/source roles) and
to inspect `nuttx.map` for the linked ISO/Mesh entry points. It now exits
successfully and records
`build1727-ble-audio-mesh.sha256`. The resolved image enables ISO
central/peripheral/broadcast, BAP unicast client and broadcast source, and
Mesh PB/relay/proxy options. The earlier static-only hash remains in
`build1727-ble-audio-mesh-compat.sha256` for traceability.

This is compile/configuration evidence only. The image was not flashed, no
LE Audio peer/ISO air interoperability was tested, and no xTS pass is claimed.
The remaining acceptance blockers are controller ISO support, a peer device,
and corrected feature-symbol checks in the wrapper.
