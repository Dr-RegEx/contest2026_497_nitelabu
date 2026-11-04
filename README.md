# ESP32-S31 openvela 开发平台

本仓提供 Function-CoreBoard-1 与 Korvo-1 的 openvela 适配、板级源码映射和开发入口。驱动与应用沿用 NuttX、apps、frameworks 的标准目录。

从 [恢复与构建](workspace/REPRODUCE.md)开始；修改入口见 [源码分层](board/SOURCE-MAP.md)，配置与镜像范围见 [配置登记表](workspace/profiles.json)和 [功能验证](docs/validation/README.md)。

| 配置 | 用途与已知范围 |
| --- | --- |
| `korvo-digit-stable` | 默认演示：原版双核摄像头数字识别，CPU1 PIE/SIMD；已再次核验固件与运行状态。独立镜像，和 Factory 分别烧录。 |
| `korvo-openvela-factory` | Korvo 单核 FLAT，原生 AM/PM/WM/MEDIA，Wi-Fi/BLE 关闭。原 Factory 镜像有摄像头、显示、基本媒体、服务与 USB 键盘记录；作为独立原生服务演示配置保留。 |
| `function-ethernet` | Function 单核 FLAT，原生 EMAC/YT8531。canonical ELF、bin 和配置与已验收版本逐字节一致，基础链路、ping、有限 TCP/UDP 接收及接口恢复通过；不是吞吐量验收。 |
| `nsh` / `demo-rmt-netapps-competition` | 基础控制台与历史网络配置。历史构建记录独立保存；Kernel 配置需要同次构建的 Kernel/AppFS。 |

[板型说明](board/ESP32-S31%20Function-CoreBoard-1%20%2B%20ESP32-S31%20Korvo-1/README.md)介绍 BSP 与资源选择；[历史源码例程](board/ESP32-S31%20Function-CoreBoard-1%20%2B%20ESP32-S31%20Korvo-1/demo/README.md)保留源码、配置、fixture 和许可。默认演示使用[原版双核数字识别固件](board/ESP32-S31%20Function-CoreBoard-1%20%2B%20ESP32-S31%20Korvo-1/firmware/korvo-digit-stable/README.md)，模型与第三方资源许可继续适用。

当前薄快照已更新 36 个改动项目、247 个 BSP 文件及依赖补丁，补丁重建与文件校验通过；完整接收端构建尚未重新验证。项目、文件及版本清单以 `workspace/snapshot.json`、锁定 manifest 和对应收据为准；入口脚本本身不构成完整公共发行包。

安装前按 [固件部署](workspace/FLASH-MANUAL.md)核对镜像与布局。验收固件可留在板上使用；原固件备份用于需要时回退。具体能力按精确镜像记录使用，音频队列溢出与长期稳定性等限制见验证索引。提交材料入口见 [SUBMISSION](docs/SUBMISSION.md)。
