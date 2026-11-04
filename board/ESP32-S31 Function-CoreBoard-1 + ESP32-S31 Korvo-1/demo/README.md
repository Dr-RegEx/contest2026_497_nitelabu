# 历史源码例程

本目录保留 87 个历史源码参考目录及其配置、许可和 C6 配套工程。例程名称、来源和配置见 [catalog.json](catalog.json)，文件身份见 [source-sha256.json](source-sha256.json)。各目录的 `profile.txt` 保留原值。

当前开发请使用恢复工程中的 canonical BSP 配置，按 [恢复与构建](../../../workspace/REPRODUCE.md)选择完整源码与依赖。这里的源码副本用于阅读和比对；不另复制一套旧环境或默认网络凭据。

[验证索引](../../../docs/validation/README.md)区分精确镜像的实板结果、历史构建与诊断。[historical.json](../../../docs/validation/historical.json)保留原测试标题、结果与限制，包括 Wi-Fi 失败、GPIO 重复启动异常和 BLE 控制器观察范围。例程数量不等于当前镜像通过数量，旧固件和原始记录不再作为本目录的构建输入。

修改或分发源码时，保留各目录原有 LICENSE、NOTICE、资源及模型许可和文件头。C6 fixture 工程仍在对应 I2C/SPI 例程中。
