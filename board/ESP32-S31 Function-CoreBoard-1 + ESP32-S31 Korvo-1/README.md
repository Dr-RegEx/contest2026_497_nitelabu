# ESP32-S31 Function-CoreBoard-1 / Korvo-1

本目录是 canonical BSP 的可读镜像。实际编译路径为 `platform-root/openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/`；工程分层见 [SOURCE-MAP](../SOURCE-MAP.md)。`src/`、`include/`、`configs/` 和构建文件共同定义板资源，不能只复制一个 defconfig。

Function 的 `function-ethernet` 使用原生 EMAC 与 YT8531；Korvo 的 `korvo-openvela-factory` 使用 camera、display、touch、SD、USB Host、ADC keys 与媒体服务，单核 FLAT 且无线关闭。配置启用某外设不等于该外设已在每个镜像验收。当前结果及镜像哈希见 [功能验证](../../docs/validation/README.md)。

Korvo 默认演示为 `korvo-factory-vision`：双核 KERNEL/MMU，上电显示 Factory 实时预览，点击 Digit recognition 进入原版 VGA/CPU1 SIMD 数字识别，点击 Return 恢复预览。该往返流程已通过实物触摸验收；本配置未启用 SD、USB Host、ADC 按键及无线，其他配置的外设结果不迁移到此镜像。

构建入口见 [恢复与构建](../../workspace/REPRODUCE.md)，修改范围见 [PATCHES](PATCHES.md)。板级镜像与 canonical 源码需在交付时按哈希同步；构建始终以 canonical 路径为准。

[demo](demo/README.md)保留历史源码、配置、C6 fixture 与许可证。[组合配置](../../workspace/FACTORY-VISION.md)从源码构建并配对部署内核/AppFS；[原版独立数字识别](firmware/korvo-digit-stable/README.md)与[独立 Factory](firmware/korvo-factory/README.md)的预编译固件继续保留。保留模型许可、OV3660 LICENSE/NOTICE 和 UI 资源归属。
