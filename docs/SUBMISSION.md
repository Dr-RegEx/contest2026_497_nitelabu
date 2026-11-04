# 交付材料导航

- [开发入口](../README.md)：板型、配置与开发流程。
- [恢复与构建](../workspace/REPRODUCE.md)：公共源码、固定依赖及独立输出目录。
- [源码分层](../board/SOURCE-MAP.md)：canonical 驱动、BSP、应用与依赖位置。
- [配置登记表](../workspace/profiles.json)与 [验证索引](validation/README.md)：具体镜像身份、通过范围和已知限制。
- [组合演示](../workspace/FACTORY-VISION.md)：Factory 主界面、原版双核识别与 Return，配对构建和烧录。
- [固件部署](../workspace/FLASH-MANUAL.md)：板卡备份、独立配置部署及可选回退。
- [历史源码例程](../board/ESP32-S31%20Function-CoreBoard-1%20%2B%20ESP32-S31%20Korvo-1/demo/README.md)：保留源码、配置、fixture 和许可；历史结果在 [historical.json](validation/historical.json)。

公共源码以锁定 manifest、`workspace/snapshot.json` 和依赖补丁恢复，覆盖 36 个改动项目。默认 Korvo 演示为 `korvo-factory-vision`，已完成 Factory 主界面进入原版双核识别、无闪烁、正常识别、Return 返回并恢复预览的人工验收，见 [组合镜像记录](validation/factory-vision.json)。组合配置通过公开入口全量构建后，显示同步修正版完成增量构建与实板验收；最终修正版尚未重跑全量构建。

仓内保留原版独立数字识别及独立 Factory 的预编译包；组合固件通过公开构建入口生成，内核和 AppFS 必须配对部署。五个独立配置的公开构建记录见 [public-reproduction.json](validation/public-reproduction.json)。Function Ethernet 与其他功能的通过范围以验证索引为准。

作品提交目标为 [官方作品仓库](https://github.com/open-vela/contest2026_497_nitelabu/tree/dev-ai-contest-2026)的 `dev-ai-contest-2026` 分支。作品仓的提交或合入不代表适配补丁已被各 openvela 上游源码仓接受。
