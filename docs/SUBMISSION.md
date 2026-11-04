# 交付材料导航

- [开发入口](../README.md)：板型、配置与开发流程。
- [恢复与构建](../workspace/REPRODUCE.md)：离线材料、平台布局及独立输出目录。
- [源码分层](../board/SOURCE-MAP.md)：canonical 驱动、BSP、应用与依赖位置。
- [配置登记表](../workspace/profiles.json)与 [验证索引](validation/README.md)：具体镜像身份、通过范围和已知限制。
- [固件部署](../workspace/FLASH-MANUAL.md)：核验、安装、保留可用固件及可选回退。
- [历史源码例程](../board/ESP32-S31%20Function-CoreBoard-1%20%2B%20ESP32-S31%20Korvo-1/demo/README.md)：保留源码、配置、fixture 和许可；历史结果在 [historical.json](validation/historical.json)。

当前公共薄快照已同步 36 个改动项目、247 个 BSP 文件与依赖补丁；默认数字识别固件及独立 Factory 固件随板级目录保存。以交付 manifest、snapshot 和收据确定文件与版本；本页不宣称公共回放已经完整验证。默认数字识别演示采用已验收原版独立固件，见 [当前数字识别记录](validation/digit-stable.json)；Function Ethernet 的基础通信范围以验证索引为准。
