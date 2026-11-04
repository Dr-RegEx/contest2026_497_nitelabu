# Wi-Fi 四方向 300 秒速率实测与失败记录

本目录是历史源码参考（historical source reference），保留源码、配置和许可。历史结论见 [验证索引](../../../../docs/validation/README.md)及 [历史记录](../../../../docs/validation/historical.json) 中的 `wifi-rate-diagnostics`，不代表当前固件验收。

配置名：`demo-wifi-rate-window64`；本地配置：[defconfig](config/demo-wifi-rate-window64/defconfig)；原配置标签：[profile.txt](profile.txt)。当前构建使用恢复工程中 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-wifi-rate-window64/defconfig`，入口见 [恢复与构建](../../../../workspace/REPRODUCE.md)。保持完整配置继承，Kernel 模式需同次构建的 Kernel/AppFS 配对。

以下来源相对于 `platform-root/openvela-dev/`：

- `external/iperf2/iperf2/src` → [src/external/iperf2/iperf2/src](src/external/iperf2/iperf2/src)
- `external/iperf2/iperf2/include` → [src/external/iperf2/iperf2/include](src/external/iperf2/iperf2/include)
- `external/iperf2/CMakeLists.txt` → [src/external/iperf2/CMakeLists.txt](src/external/iperf2/CMakeLists.txt)
- `external/iperf2/Kconfig` → [src/external/iperf2/Kconfig](src/external/iperf2/Kconfig)
- `nuttx/arch/risc-v/src/common/espressif/esp_wlan.c` → [src/nuttx/arch/risc-v/src/common/espressif/esp_wlan.c](src/nuttx/arch/risc-v/src/common/espressif/esp_wlan.c)

许可与归属：[licenses/external/LICENSE](licenses/external/LICENSE), [licenses/iperf2/COPYING](licenses/iperf2/COPYING), [licenses/iperf2/LICENSE](licenses/iperf2/LICENSE), [licenses/nuttx/LICENSE](licenses/nuttx/LICENSE), [licenses/nuttx/NOTICE](licenses/nuttx/NOTICE)。源码中的版权声明和资源授权继续适用。
