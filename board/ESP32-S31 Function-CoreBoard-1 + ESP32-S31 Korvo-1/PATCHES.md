# 修改与源码定位

补丁和新增文件以 [snapshot.json](../../workspace/snapshot.json)、[锁定 manifest](../../workspace/manifest-locked.xml)及其校验清单为准。`workspace/patches/`、`workspace/openvela-untracked/` 和 `workspace/dependencies/` 是源码恢复载荷；项目数从相应清单读取。

| 修改内容 | canonical 位置（相对于 platform-root） |
| --- | --- |
| BSP、引脚、配置继承、设备注册 | `openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/` |
| S31 驱动、启动与内存 | `openvela-dev/nuttx/arch/risc-v/src/esp32s31/` |
| Espressif 共用适配 | `openvela-dev/nuttx/arch/risc-v/src/common/espressif/` |
| HAL / SDMMC 依赖 | `openvela-dev/.s31-deps/esp-hal-3rdparty/`；作品仓 `workspace/dependencies/idf-sdmmc/` |
| Factory UI、assets 与生命周期 | `openvela-dev/frameworks/runtimes/services/xmsdemo/factory/` |
| 原生服务 | `openvela-dev/frameworks/runtimes/services/`、`openvela-dev/frameworks/multimedia/media/` |

详细入口见 [源码分层](../SOURCE-MAP.md)。更改板资源前核对 pinmux、ADC/RF 与外设共享约束。保留 LICENSE、NOTICE、UPSTREAM、模型及资源许可，不按测试名称删除源码依赖。

公共快照与固定依赖按 [恢复与构建](../../workspace/REPRODUCE.md)准备。五个独立配置已有全量构建记录；Factory 与原版识别组合配置已有公开全量构建及显示修正版的增量构建、实物触摸往返验收，最终修正版尚未重跑全量构建。[验证索引](../../docs/validation/README.md)记录对应镜像与范围，历史记录不自动适用于新编译输出。
