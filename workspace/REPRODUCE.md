# 从公共仓库复现

已验证宿主为 Ubuntu 22.04、Linux x86_64、Python 3.10。源码目录必须区分大小写；WSL 建议将源码和依赖放在 Linux 文件系统，避免 Windows 挂载盘上的大量头文件读取拖慢编译；建议至少 20 GiB 空闲空间和 8 GiB 内存。网络需要访问 GitHub、Gitee、PyPI。源码使用锁定的 232 个 openvela 项目与本仓补丁，不依赖开发者原电脑。

## 1. 拉取和准备

按根 README 第三节安装系统包、执行 repo init / sync，并在干净固定基线中执行一次 `apply-openvela-snapshot.sh`。不要用其他分支替换 manifest 内的提交，也不要在已有开发树上反复应用补丁。

```bash
CONTEST_REPO=/path/to/checkout/contest2026_497_nitelabu
CHECKOUT=/path/to/checkout
bash "$CONTEST_REPO/workspace/setup-dependencies.sh" "$CHECKOUT"
```

脚本创建 `CHECKOUT/.s31-deps`：HAL 固定提交及其子模块、两份仓内补丁、新 Python 虚拟环境、固定 RISC-V 工具链、主机生成器运行库。所有下载都有明确上游地址；工具链按 SHA256 校验。仅下载依赖需要网络，构建阶段不通过 FetchContent 拉取项目。

可以显式传入 `--deps /path/to/dependencies` 与 `--cache /path/to/downloaded-archives`。缓存只减少下载，仍核对归档 SHA256；它不允许替换其他版本。使用自定义依赖目录时，构建也传同一 `--deps`。脚本支持依赖准备失败后重试，不会切换已有 HAL 的提交。已有 HAL 版本不符时请使用新的目录。

SDMMC 所需的七份 ESP-IDF 头文件来自固定公开提交，位于 `dependencies/idf-sdmmc`；它们不要求安装整个 ESP-IDF。两份 Ubuntu 运行库保存在 `dependencies/host-packages`，由 dpkg-deb 解到依赖目录，不安装到系统。对应许可证和来源随文件保留。

## 2. 构建

```bash
bash "$CONTEST_REPO/workspace/build.sh" --workspace "$CHECKOUT" \
  --profile nsh --output /path/to/new-nsh-output --jobs 4
bash "$CONTEST_REPO/workspace/build.sh" --workspace "$CHECKOUT" \
  --profile function-ethernet --output /path/to/new-ethernet-output --jobs 4
bash "$CONTEST_REPO/workspace/build.sh" --workspace "$CHECKOUT" \
  --profile korvo-openvela-factory --output /path/to/new-factory-output --jobs 4
bash "$CONTEST_REPO/workspace/build.sh" --workspace "$CHECKOUT" \
  --profile korvo-factory-vision --output /path/to/new-factory-vision-output --jobs 4
bash "$CONTEST_REPO/workspace/build.sh" --workspace "$CHECKOUT" \
  --profile korvo-digit-rebuild --output /path/to/new-digit-output --jobs 4
bash "$CONTEST_REPO/workspace/build.sh" --workspace "$CHECKOUT" \
  --profile demo-rmt-netapps-competition --output /path/to/new-netapps-output --jobs 4
```

输出目录不能已经存在，也不能包含或位于源码、依赖、作品仓中。每次使用新目录，失败目录保留日志。配置与编译日志为 `configure.log`、`build.log`，退出码和镜像校验值在 `receipt.json`，固件在 `build/nuttx.bin`。`--dry-run` 仅检查输入和显示命令，不代表编译通过。

最小配置用于启动与 UART shell；有线网配置用于 Function-CoreBoard；Factory 配置用于 Korvo 屏幕、摄像头和系统服务。不同配置的内核与 AppFS 不能混用。Flash 布局及预期实板现象见 [部署说明](FLASH-MANUAL.md)和各固件目录 README。

`korvo-factory-vision` 为双核组合配置：Factory 主界面通过按钮进入原版数字识别并返回。其内核与 AppFS 配对布局见 [Factory 与原版识别](FACTORY-VISION.md)。

## 3. 失败排查

- `Wrong baseline`：重新按固定 manifest 拉取干净源码，不绕过补丁检查。
- HAL 或工具链下载失败：确认网络可访问日志中的公开地址，重试依赖准备。无需提供开发者电脑路径。
- `PUBLIC_DEPENDENCIES_READY` 缺失：依赖准备未完成，查看安装日志并重试。
- AIDL / JIDL 运行失败：检查 manifest 的 `prebuilts` 是否齐全；本入口已设置所需运行库路径。
- 编译失败：保留完整输出目录，先检查 `configure.log` 和 `build.log`，不要混用旧 Make 产生的配置或头文件。

## 4. 已有离线包

旧离线恢复工具仍保留：`restore-offline.py` 和 `build-offline.py` 接受原有产品/SDK 布局。它们只服务于已经持有该离线包的用户，公共复现流程无需这些包。公开构建命令统一使用本页的 `setup-dependencies.sh` 和 `build.sh --workspace`。
