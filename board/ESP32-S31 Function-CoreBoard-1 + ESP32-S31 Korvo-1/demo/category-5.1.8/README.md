# 文件系统多线程写文件测试vela_fs_multi_thread_write_test

本目录是历史源码参考（historical source reference），保留源码、配置和许可。历史结论见 [验证索引](../../../../docs/validation/README.md)及 [历史记录](../../../../docs/validation/historical.json) 中的 `category-5.1.8`，不代表当前固件验收。

配置名：`xts-flat-category-fs-name`；本地配置：[defconfig](config/xts-flat-category-fs-name/defconfig)；原配置标签：[profile.txt](profile.txt)。当前构建使用恢复工程中 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/xts-flat-category-fs-name/defconfig`，入口见 [恢复与构建](../../../../workspace/REPRODUCE.md)。保持完整配置继承，Kernel 模式需同次构建的 Kernel/AppFS 配对。

以下来源相对于 `platform-root/openvela-dev/`：

- `tests/testcases/vela_fs_test` → [src/vela_fs_test](src/vela_fs_test)

许可与归属：[licenses/tests/LICENSE](licenses/tests/LICENSE)。源码中的版权声明和资源授权继续适用。
