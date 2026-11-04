# NetApp--设备支持通过scp方式从其他终端获取文件到本地

本目录是历史源码参考（historical source reference），保留源码、配置和许可。历史结论见 [验证索引](../../../../docs/validation/README.md)及 [历史记录](../../../../docs/validation/historical.json) 中的 `category-4.1.115`，不代表当前固件验收。

配置名：`demo-rmt-netapps-competition`；本地配置：[defconfig](config/demo-rmt-netapps-competition/defconfig)；原配置标签：[profile.txt](profile.txt)。当前构建使用恢复工程中 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-rmt-netapps-competition/defconfig`，入口见 [恢复与构建](../../../../workspace/REPRODUCE.md)。保持完整配置继承，Kernel 模式需同次构建的 Kernel/AppFS 配对。

以下来源相对于 `platform-root/openvela-dev/`：

- `external/libssh/CMakeLists.txt` → [src/CMakeLists.txt](src/CMakeLists.txt)
- `external/libssh/libssh/examples` → [src/examples](src/examples)
- `external/libssh/Kconfig` → [src/Kconfig](src/Kconfig)

许可与归属：[licenses/external/LICENSE](licenses/external/LICENSE), [licenses/external/libssh/libssh/COPYING](licenses/external/libssh/libssh/COPYING)。源码中的版权声明和资源授权继续适用。
