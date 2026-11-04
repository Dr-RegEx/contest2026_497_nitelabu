# 开发者源码分层

以下路径以根 README 创建的 `platform-root/openvela-dev/` 为例。`openvela-dev/` 保留 openvela / NuttX 原目录层次；公开依赖默认在 checkout 的 `.s31-deps/`，构建入口与随仓工具位于作品仓 `workspace/`。

| 修改目标 | 源码入口 |
| --- | --- |
| 板型、引脚、配置、启动与设备注册 | `openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/` |
| 板资源冲突 | BSP 的 `src/esp32s31_board_resources.h`、`src/esp32s31_bringup.c` 与 `include/board.h` |
| 芯片驱动 | `openvela-dev/nuttx/arch/risc-v/src/esp32s31/`；共用代码在 `arch/risc-v/src/common/espressif/` |
| HAL / SDMMC 头文件 | `openvela-dev/.s31-deps/esp-hal-3rdparty/`；随仓头文件在 `contest2026_497_nitelabu/workspace/dependencies/idf-sdmmc/`，无需完整 IDF SDK |
| 通用示例 | `openvela-dev/apps/examples/{s31preview,s31demo,s31keys,s31simd}/` |
| Factory UI 与切换控制 | `openvela-dev/frameworks/runtimes/services/xmsdemo/factory/` |
| 原版双核识别及设备交接 | BSP 的 `src/s31vision.c`、`src/vision_*.h`、`src/s31vision_ops.S`；独立版覆盖层在作品仓 `workspace/digit-demo/` |
| AM / PM / WM 与媒体 | `openvela-dev/frameworks/runtimes/services/{am,pm,wm}/`、`openvela-dev/frameworks/multimedia/media/` |
| 公共构建与主机工具 | 作品仓 `workspace/build.sh`、`workspace/tools/`；芯片镜像工具在 `openvela-dev/nuttx/tools/espressif/` |

比赛仓的板目录是 BSP 可读镜像。manifest linkfile 将它展示为 `vendor/openvela/boards/contest2026_497_board`；编译读取上述 canonical BSP，无须手工复制到 vendor。镜像同步需以 canonical 的 Kconfig、构建文件、configs/include/scripts/src 文件哈希核对。

`korvo-openvela-factory` 是独立的单核 FLAT 配置；`korvo-factory-vision` 是双核 KERNEL/MMU 组合配置。后者由 Factory 按钮启动原版内核数字识别任务并在 Return 后恢复预览，接口和部署见 [组合配置说明](../workspace/FACTORY-VISION.md)，精确镜像验收状态见 [配置登记表](../workspace/profiles.json)。修改 camera、touch、audio 或显示驱动时检查资源所有权。原独立双核 digit 及其构建配置继续保留。

保留 `ov3660.LICENSE` / `ov3660.NOTICE`、Factory 的 `manifest.json` / `UPSTREAM.json` / `LICENSES.md`、PNG、字体、coffee 资源和模型许可。配置闭包依赖的测试源码也属于恢复载荷。

[验证索引](../docs/validation/README.md)集中说明镜像身份与历史边界；[恢复与构建](../workspace/REPRODUCE.md)说明源码包、依赖和薄快照状态。
