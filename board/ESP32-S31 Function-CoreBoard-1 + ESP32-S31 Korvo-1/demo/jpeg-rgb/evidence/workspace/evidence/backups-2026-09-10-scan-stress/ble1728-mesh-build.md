# 1728 BLE Mesh build candidate

- Date: 2026-09-18
- Profile: `xts-flat-ble-mesh`
- Image: `openvela-dev/out/esp32s31-xts-flat-ble-mesh-1728/nuttx.bin`
- Receipt: `build1728-ble-mesh.sha256`
- Image size: 1,294,392 bytes
- Image SHA-256: `34b7a1eb7afaa575753da56044ea2b38fbf4c94cb7849d8bdfc7309c20372c3f`

The isolated build completed all 1,788 Ninja targets. `.config` contains
`CONFIG_BT_MESH`, PB-ADV, PB-GATT, provisionee, relay, GATT proxy and related
Mesh options. `build.ninja` includes the ZBlue Mesh objects, including
`mesh/main.c.o`, `adv_ext.c.o`, `pb_gatt.c.o`, and `crypto_psa.c.o`.

Compatibility fixes needed for this candidate were confined to the checked-in
ZBlue sources: the ISO connection lookup now uses `conn->hdev`, Mesh extended
advertising uses `advs[0]`, the provisioning state avoids the POSIX `link`
name collision, and the Mesh PSA random hook is guarded when the original BT
API supplies `bt_rand`.

This is compile/static evidence only. The image was not flashed, no second
Mesh node was available for provisioning or model traffic, and no xTS count
was increased. BLE Mesh 1.1 therefore remains pending runtime validation.

