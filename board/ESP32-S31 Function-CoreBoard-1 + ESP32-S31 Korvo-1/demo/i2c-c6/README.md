# I2C 通用主从通信（S31 ↔ C6）

本目录是历史源码参考（historical source reference），保留源码、配置和许可。历史结论见 [验证索引](../../../../docs/validation/README.md)及 [历史记录](../../../../docs/validation/historical.json) 中的 `i2c-c6`，不代表当前固件验收。

配置名：`xts-flat-i2c-c6`；本地配置：[defconfig](config/xts-flat-i2c-c6/defconfig)；原配置标签：[profile.txt](profile.txt)。当前构建使用恢复工程中 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/xts-flat-i2c-c6/defconfig`，入口见 [恢复与构建](../../../../workspace/REPRODUCE.md)。保持完整配置继承，Kernel 模式需同次构建的 Kernel/AppFS 配对。

以下来源相对于 `platform-root/openvela-dev/`：

- `apps/system/i2c` → [src/i2c](src/i2c)

配套 C6 工程保留在 [c6](c6/)，来源为 `workspace/tools/esp32c6-i2c-slave`。

许可与归属：[licenses/apps/LICENSE](licenses/apps/LICENSE), [licenses/apps/NOTICE](licenses/apps/NOTICE)。源码中的版权声明和资源授权继续适用。
