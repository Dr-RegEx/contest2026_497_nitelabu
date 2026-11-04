# GPIO47/48 实物回环与上升沿中断

本目录是历史源码参考（historical source reference），保留源码、配置和许可。历史结论见 [验证索引](../../../../docs/validation/README.md)及 [历史记录](../../../../docs/validation/historical.json) 中的 `common-1.3.6`，不代表当前固件验收。

配置名：`demo-gpio-loopback`；本地配置：[defconfig](config/demo-gpio-loopback/defconfig)；原配置标签：[profile.txt](profile.txt)。当前构建使用恢复工程中 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-gpio-loopback/defconfig`，入口见 [恢复与构建](../../../../workspace/REPRODUCE.md)。保持完整配置继承，Kernel 模式需同次构建的 Kernel/AppFS 配对。

以下来源相对于 `platform-root/openvela-dev/`：

- `apps/testing/drivers/drivertest` → [src/apps/testing/drivers/drivertest](src/apps/testing/drivers/drivertest)
- `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_xts_gpio.c` → [src/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_xts_gpio.c](src/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_xts_gpio.c)

许可与归属：[licenses/apps/LICENSE](licenses/apps/LICENSE), [licenses/apps/NOTICE](licenses/apps/NOTICE), [licenses/nuttx/LICENSE](licenses/nuttx/LICENSE), [licenses/nuttx/NOTICE](licenses/nuttx/NOTICE)。源码中的版权声明和资源授权继续适用。
