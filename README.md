# ESP32-S31 Function-CoreBoard-1 openvela 适配

## 一、作品简介

为 ESP32-S31 Function-CoreBoard-1 移植 openvela，包含 SMP/MMU、外部应用加载、Wi-Fi、存储、音频、BLE 及外设适配与测试配置。选题方向为**新硬件适配**。

本仓为团队复刻仓 `Dr-RegEx/contest2026_497_nitelabu`，用于成员模拟评委拉取、构建和审核。此次重建以官方模板为起点，不向官方比赛仓库发起 PR。源码快照日期：2026-09-19。

## 二、目录结构

- `board/ESP32-S31 Function-CoreBoard-1/`：真实板级源码、Kconfig 和各测试配置，替换空模板。
- `workspace/manifest-locked.xml`：232 个 openvela 公共源码项目的固定版本。
- `workspace/patches/`、`workspace/openvela-untracked/`：公共基线之上的全部适配提交、工作区修改和新增源码。
- `workspace/dependencies/`：HAL 修改、工具链下载地址与 SHA256、Python 依赖版本。
- `workspace/tools/`：S31 主机检查及 C6 I2C/SPI 辅助板工程。
- `docs/acceptance/`、`workspace/evidence/`：当前验收清单和历史测试证据。
- `docs/reproduction/`：此次独立源码目录构建的结果、日志和产物哈希。
- `logs/`：选定真实 AI 会话的导出记录及来源说明。

## 三、拉取完整工程

环境：Ubuntu 22.04 / Linux x86_64（本次验证环境）；Linux aarch64 工具链下载也已配置，尚未验证。需要访问 GitHub、Gitee 和 PyPI。

```bash
sudo apt-get update
sudo apt-get install -y git git-lfs repo python3 python3-venv python3-pip \
  cmake ninja-build build-essential bison flex gperf gettext texinfo \
  libncurses-dev libssl-dev genromfs xz-utils unzip patch curl
mkdir s31-review && cd s31-review
repo init -u https://github.com/Dr-RegEx/contest2026_497_nitelabu.git \
  -b dev-ai-contest-2026 -m contest2026_497_nitelabu.xml
GIT_LFS_SKIP_SMUDGE=1 repo sync -c -j8
./contest2026_497_nitelabu/workspace/apply-openvela-snapshot.sh "$PWD"
./contest2026_497_nitelabu/workspace/setup-dependencies.sh "$PWD"
```

源码基线来自 **Gitee 官方 open-vela 镜像**：本项目使用的提交与 GitHub 镜像并不相同，不能直接替换域名。跳过 LFS 下载的是其他厂商板型的预编译库，S31 不使用它们；S31 HAL 的 Wi-Fi/PHY/BLE 子模块由准备脚本单独拉取。

补丁脚本先校验所有基线、补丁和新增文件，再写入。它只接受干净的固定版本工程；同一工作区只运行一次。失败时会明确报错，不会把已有源码静默跳过。不要使用旧的 `netlab` manifest 或旧板级补丁。

## 四、编译、烧录与运行

在 `s31-review/` 工作区根目录执行：

```bash
# 最小 NSH：约 214 KiB，单镜像，首次复现建议从此开始
./contest2026_497_nitelabu/workspace/build.sh "$PWD" nsh 8

# SMP/MMU、网络和外部应用：生成同目录配套的 nuttx.bin + appfs.img
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-netapps-competition 8

# Camera/V4L2：仅构建验证；外接摄像头实测尚未完成
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-camera 8
```

产物位于 `out/esp32s31-<配置>/`。这些配置本次均在独立目录从公共基线加补丁构建成功，详见 [复现验证记录](docs/reproduction/VALIDATION.md)。使用 CMake/Ninja 专用入口；不要在同一源码树混入 Make 生成的 `.config` 或 `include/nuttx/config.h`。

首次运行 NSH，连接目标板 USB 串口，确认实际端口并让板进入下载模式；以下命令由操作板卡的队员执行。本次上传工作没有刷写开发板。

```bash
# 把 /dev/ttyACM0 替换成实际 S31 端口，勿选 C6 辅助板
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  read-flash 0 0x1000000 original-flash.bin
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash 0x2000 out/esp32s31-nsh/nuttx.bin
.s31-deps/venv/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200
```

复位后应进入 `nsh>`，可执行 `uname -a`、`help`。Kernel 模式必须烧写同一次构建的内核与 AppFS，不能拿历史镜像混配；详细步骤见 [复现与审核说明](workspace/REPRODUCE.md)。

## 五、验收状态与审核边界

以 [比赛必须适配清单](docs/acceptance/比赛必须适配清单.md) 为逐项入口。历史项目按原记录保留通过、用户确认、开发验证和待实物验收的区别。编译成功不等于 xTS 实板通过；Camera、USB、SDMMC、BLE 等能力的具体状态以对应记录为准。

历史脚本和日志保留当时的绝对路径、配置与上下文，归档在 `workspace/evidence/`；它们不是新的构建入口。公开副本中移除了测试凭据，修改文件列表见 `workspace/evidence/REDACTIONS.json`；原始本地证据未改写。

## 六、AI Coding 使用说明

AI 用于移植方案分析、编码、错误定位、脚本整理和复现审查；最终验收按构建输出、真实串口记录及人工确认分别记录。本次重建发现并修复了未发布提交作为 manifest 基线、公共仓路径推导错误、HAL 缺失和不支持 S31 的 esptool 版本等问题。

`logs/` 已导出本次复现修复会话的真实用户/助手可见对话，使用官方采集器写入器并通过官方校验；它是选定会话的阶段性导出，**不代表全部历史 AI 会话已归集**。工具输出、系统指令及内部推理不在选定导出范围内，详见 [日志说明](logs/README.md)。正式比赛提交前仍需队员归集各自历史会话，并按官方规则处理公共仓 PR 与比赛仓 PR；本次只更新复刻仓。
