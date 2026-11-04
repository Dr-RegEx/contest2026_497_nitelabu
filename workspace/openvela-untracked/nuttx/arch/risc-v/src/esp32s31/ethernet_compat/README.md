# ESP32-S31 Ethernet HAL compatibility sources

These four implementations belong to the native NuttX ESP32-S31 Ethernet
port. They retain their original Espressif copyright and Apache-2.0 SPDX
license notices. `hal_esp32s31.cmake` selects them only for this chip's
`CONFIG_ESPRESSIF_EMAC` build. Other chip ports continue to use the unchanged
implementations under the shared `esp-hal-3rdparty/components/esp_eth` tree.

The files are byte-for-byte copies of the Function Ethernet candidate with
the APLL ownership fix. Relocation changes source selection and the private
header search path only. It preserves NuttX RX notification, synchronization,
DMA, PHY behavior, clock acquisition/release, and existing diagnostics.
The native driver remains `../esp32s31_emac.c` and `../esp32s31_emac.h`;
board wiring and the Function profile remain in the canonical NuttX BSP.

| File | Original HAL path below `components/esp_eth` | Implementation SHA-256 |
| --- | --- | --- |
| esp_eth_mac_esp.c | src/mac/esp_eth_mac_esp.c | 7c847887b2b2de6add5ebd213dc0dd540ea6348f5d1bc669f0a5adcb6a35b200 |
| esp_eth_mac_esp_dma.c | src/mac/esp_eth_mac_esp_dma.c | e0b2dc31d8707fa64f07e98735ca4eaf7946abb5acc071a3b1d397135ebe4a93 |
| esp_eth_phy_802_3.c | src/phy/esp_eth_phy_802_3.c | c74e8880ce9cb8263ca0757ee2945db22d436116a8f38228706fdf0e93c001bb |
| esp_eth_phy_generic.c | src/phy/esp_eth_phy_generic.c | ed7b4501ac76317f6a49f84cb949481ab3190eadf140cdbb3667e14779fa253b |

## Pinned HAL dependency

These are adapted implementations, not a standalone Ethernet library.
They require `esp-hal-3rdparty` revision
`290edc31b50decca660c1a11ce3506fd9b2e1e27`, already enforced by
`../esp32s31_espressif.cmake`. No HAL headers are copied or upgraded here.
The build keeps the HAL's public `components/esp_eth/include` headers,
including `esp_eth_phy_802_3.h` and `esp_private/eth_mac_esp_dma.h`, and its
private `components/esp_eth/src/mac/emac_priv.h`. It also keeps the existing
EMAC HAL/LL, GPIO, clock-tree, SoC and NuttX adaptation headers and the
unchanged shared GPIO/EMAC support sources selected by `hal_esp32s31.cmake`.
The private MAC include directory is explicitly supplied by that S31 build
file because `emac_priv.h` is no longer adjacent to the adapted MAC source.

Any future HAL revision change needs a joint review of these source copies
and the public/private header contracts, followed by a rebuild and hardware
qualification. A successful relocation build alone is not hardware acceptance.
