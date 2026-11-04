# 原版双核数字识别源码构建

此目录恢复原先的 VGA 摄像头、CPU0 显示、CPU1 本地数字识别演示，包含原页面、模型、预处理与 SIMD 推理代码。`korvo-digit-rebuild` 生成独立识别镜像；需要从 Factory 主界面进入原版识别并返回时，使用 [korvo-factory-vision 组合配置](../FACTORY-VISION.md)。组合版复用原版模型、预处理、字体和 SIMD 内核，其设备生命周期与显示切换适配位于平台 BSP。

先按根 README 拉取固定 openvela 源码、应用平台补丁并运行公开依赖安装，然后执行：

```bash
bash "$CONTEST_REPO/workspace/build.sh" --workspace "$CHECKOUT" \
  --profile korvo-digit-rebuild --output /path/to/new-digit-build --jobs 4
```

若安装依赖时指定了 `--deps`，构建时传入同一个目录。脚本在新输出目录复制 NuttX/apps，应用经过校验的原版源码覆盖层，并创建独立 HAL 适配副本；不修改平台的 Factory 或有线网源码。其余 openvela 项目与固定子模块从公开 checkout 读取。请不要在构建期间修改该 checkout。额外需要约 2 GiB 空间。

`overlay/` 是相对于公共平台快照的原版源文件差异，包含识别演示及其对应的摄像、显示、板级启动和调度器实现。原版修改前备份用于恢复后续生命周期与内存管理实验之前的代码。`hal.patch` 只应用于干净的 HAL 固定提交，不能叠在平台 HAL 补丁之上；脚本自动处理这一点。`sha256.json` 校验全部覆盖文件。源码中的版权和 SPDX 声明保留，公共基线和 HAL 的许可证继续适用。

编译结果在 `OUTPUT/build/nuttx.bin`，配置与 ELF 同目录，完整构建日志和校验值在输出根目录。该配置为 16 MiB Korvo 的 simple boot，内核偏移 `0x2000`，不需要 AppFS。使用公开依赖目录的 `venv/bin/esptool`，按照原版固件目录说明先备份，再烧写并验证新镜像。

原已验收固件仍保存在 `board/.../firmware/korvo-digit-stable`。新构建会含新的时间、路径和版本元数据，不承诺与历史镜像逐字节一致；构建验证与新镜像实板结果分别记录。
