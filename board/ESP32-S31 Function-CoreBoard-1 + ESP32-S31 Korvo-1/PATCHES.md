# 修改与源码定位

补丁和新增文件以 [snapshot.json](../../workspace/snapshot.json)、[锁定 manifest](../../workspace/manifest-locked.xml)及其校验清单为准。`workspace/patches/`、`workspace/openvela-untracked/` 和 `workspace/dependencies/` 是源码恢复载荷；项目数从相应清单读取。

| 修改内容 | canonical 位置（相对于 platform-root） |
| --- | --- |
| BSP、引脚、配置继承、设备注册 | `openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/` |
| S31 驱动、启动与内存 | `openvela-dev/nuttx/arch/risc-v/src/esp32s31/` |
| Espressif 共用适配 | `openvela-dev/nuttx/arch/risc-v/src/common/espressif/` |
| HAL / IDF 依赖 | `s31-reference/deps/esp-hal-3rdparty/`、`s31-reference/tmp/esp-idf-clean/` |
| Factory UI、assets 与生命周期 | `openvela-dev/frameworks/runtimes/services/xmsdemo/factory/` |
| 原生服务 | `openvela-dev/frameworks/runtimes/services/`、`openvela-dev/frameworks/multimedia/media/` |

详细入口见 [源码分层](../SOURCE-MAP.md)。更改板资源前核对 pinmux、ADC/RF 与外设共享约束。保留 LICENSE、NOTICE、UPSTREAM、模型及资源许可，不按测试名称删除源码依赖。

当前公共薄快照尚待整体刷新和独立回放。按 [恢复与构建](../../workspace/REPRODUCE.md)使用对应源码包与依赖；[验证索引](../../docs/validation/README.md)记录精确镜像的结果，历史记录不自动适用于新编译输出。
