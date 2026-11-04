# ESP32-S31 openvela 开发板适配

## 一、作品简介

为 ESP32-S31 开发平台适配 openvela，包含 SMP/MMU、外部应用加载、Wi-Fi、有线网络、存储、音频、BLE 及外设适配与测试配置。

选题方向：**新硬件适配**

开发板：ESP32-S31 Function-CoreBoard-1、ESP32-S31 Korvo-1

本仓为队伍 497（nitelabu）的新硬件适配作品。源码版本以锁定 manifest 和 `workspace/snapshot.json` 为准；各配置的功能与镜像范围见 [配置登记表](workspace/profiles.json)。

## 二、目录结构

- `board/ESP32-S31 Function-CoreBoard-1 + ESP32-S31 Korvo-1/`：真实板级源码、Kconfig 和各测试配置。
- [测试例程](board/ESP32-S31%20Function-CoreBoard-1%20%2B%20ESP32-S31%20Korvo-1/demo/README.md)：87 个历史例程，保留逐项 README、源码、配置及许可，验证范围见对应记录。
- `board/ESP32-S31 Function-CoreBoard-1 + ESP32-S31 Korvo-1/firmware/`：原版双核数字识别与独立 Factory 演示固件、配置、校验值和部署说明。
- `workspace/manifest-locked.xml`：232 个 openvela 公共源码项目的固定版本。
- `workspace/patches/`、`workspace/openvela-untracked/`：公共基线之上的适配补丁及源码，覆盖 36 个改动项目。
- `workspace/dependencies/`：HAL 修改、工具链下载地址与 SHA256、Python 依赖版本。
- `workspace/tools/`：S31 主机检查、构建恢复工具及 C6 I2C/SPI 辅助板工程。
- `docs/validation/`：功能验证索引、精确镜像记录和历史测试结果。
- `workspace/REPRODUCE.md`、`workspace/FLASH-MANUAL.md`：工程恢复、构建与部署说明。
- `logs/`：选定真实 AI 会话的导出记录及来源说明。

## 三、拉取完整工程

环境：Ubuntu 22.04 / Linux x86_64（已验证环境）；其他宿主尚未验证。在线拉取需要访问 GitHub、Gitee 和 PyPI。

```bash
sudo apt-get update
sudo apt-get install -y git git-lfs repo python3 python3-venv python3-pip \
  cmake ninja-build build-essential bison flex gperf gettext texinfo \
  libncurses-dev libssl-dev genromfs xz-utils unzip patch curl rsync
mkdir -p s31-review/openvela-dev && cd s31-review/openvela-dev
repo init -u https://github.com/open-vela/contest2026_497_nitelabu.git \
  -b dev-ai-contest-2026 -m contest2026_497_nitelabu.xml
GIT_LFS_SKIP_SMUDGE=1 repo sync -c -j8
CONTEST_REPO="$PWD/contest2026_497_nitelabu"
PLATFORM_ROOT="$(dirname "$PWD")"
bash "$CONTEST_REPO/workspace/apply-openvela-snapshot.sh" "$PWD"
```

源码基线来自 **Gitee 官方 open-vela 镜像**：本项目使用的提交与 GitHub 镜像并不相同，不能直接替换域名。跳过 LFS 下载的是其他厂商板型的预编译库，S31 不使用它们。

补丁脚本先校验所有基线、补丁和文件，再写入。它只接受干净的固定版本工程；同一工作区只运行一次。失败时会明确报错，不会把已有源码静默跳过。不要使用旧的 `netlab` manifest 或旧板级补丁。

上述命令完成源码拉取与补丁应用，不包含完整 SDK 准备。构建还需要匹配的 HAL/IDF、Python 环境、主机生成器和工具链；按 [工程恢复说明](workspace/REPRODUCE.md)准备 `platform-root/tools`、`platform-root/s31-reference` 及独立 SDK。旧 `setup-dependencies.sh` 的 `.s31-deps` 布局不能直接替代当前构建入口所需环境。已有离线产品与 SDK 包的恢复方式也见该文档；Git 仓库本身不包含完整 SDK。

## 四、编译、烧录与运行

在第三节的环境准备完成后，设置实际 SDK 和独立输出目录：

```bash
SDK=/path/to/restored-sdk
BUILD_OUTPUT=/path/to/new-build-output

bash "$CONTEST_REPO/workspace/build.sh" --list

# Korvo 原生 Factory：摄像头主面板、咖啡机界面及系统服务
bash "$CONTEST_REPO/workspace/build.sh" \
  --platform-root "$PLATFORM_ROOT" --sdk "$SDK" \
  --profile korvo-openvela-factory --output "$BUILD_OUTPUT" --jobs 8
```

最小 NSH 使用 `--profile nsh`；SMP/MMU、网络和外部应用使用 `--profile demo-rmt-netapps-competition`；Function 有线网使用 `--profile function-ethernet`。每次构建指定新的输出目录。旧 Factory 离线产品包不含有线网配置，构建该配置需使用完整对应源码及依赖。

产物位于 `BUILD_OUTPUT/build/`。使用 CMake/Ninja 专用入口；不要在同一源码树混入 Make 生成的 `.config` 或 `include/nuttx/config.h`。原 Factory 产品已完成独立接收端构建，有线网 canonical ELF、bin 和配置与实测版本逐字节一致；当前公共薄快照已通过补丁重建及载荷校验，完整新环境构建尚未重新验证。详见 [工程恢复说明](workspace/REPRODUCE.md)。

Korvo 默认演示采用[原版双核数字识别固件](board/ESP32-S31%20Function-CoreBoard-1%20%2B%20ESP32-S31%20Korvo-1/firmware/korvo-digit-stable/README.md)，上电自动显示摄像头画面及识别结果；[Factory 演示](board/ESP32-S31%20Function-CoreBoard-1%20%2B%20ESP32-S31%20Korvo-1/firmware/korvo-factory/README.md)独立保留。两者分别烧录，未实现按钮跳转。原版数字识别目录保留稳定固件与部分源码快照，不宣称能够独立重建该稳定镜像。

烧录前连接目标板 USB-UART，确认实际端口并保存本板原固件备份。按对应固件目录的校验与烧录说明操作；不要混用不同配置的 Flash 布局。通用 Factory 部署与回退步骤见 [固件部署](workspace/FLASH-MANUAL.md)。

最小 NSH 配置复位后应进入 `nsh>`，可执行 `uname -a`、`help`。Kernel/AppFS 配置必须烧写同一次构建的内核与 AppFS，不能拿历史镜像混配；原版数字识别固件不需要额外 AppFS。

## 五、验收状态与审核边界

以 [功能验证](docs/validation/README.md)和[测试例程](board/ESP32-S31%20Function-CoreBoard-1%20%2B%20ESP32-S31%20Korvo-1/demo/README.md)为逐项入口。历史项目按原记录保留通过、用户确认、开发验证和待实物验收的区别。编译成功不等于 xTS 实板通过；各项结果只适用于对应配置和精确镜像。

Korvo 原版数字识别已有真实画面及识别的人工确认，已再次确认固件身份、双核启动、SIMD 自检和摄像/显示/推理进度。Factory 的摄像头主面板、咖啡机界面、USB 键盘及原生服务已有专项记录；音频仍存在队列溢出和听感验收边界。Function 有线网的链路、ping、TCP/UDP 接收及接口恢复通过，1 Gbps 为链路协商速率，不代表已测得千兆吞吐量。

USB、SDMMC、BLE 等能力的具体状态以对应记录为准。QuickJS/Feature 等服务基础验证不等于完整 Quick App/RPK 运行验收。历史测试结果见 [历史记录](docs/validation/historical.json)；原始日志和完整本地备份保留在本地，不作为公共仓库构建输入。

## 六、AI Coding 使用说明

AI 用于移植方案分析、编码、错误定位、脚本整理和复现审查；最终验收按构建输出、真实串口记录及人工确认分别记录。复现审查发现并修复了未发布提交作为 manifest 基线、公共仓路径推导错误、HAL 缺失和不支持 S31 的 esptool 版本等问题。

`logs/` 包含 2026-09-03 至 2026-11-04 的 184 个真实会话、1,647 条用户/助手事件，使用官方格式并通过官方校验。保留的会话正文、时间戳和序号未改写；它是选定会话的导出，**不代表全部历史 AI 会话**。工具输出、系统指令及内部推理不在选定导出范围内，详见 [日志说明](logs/README.md)。公共仓改动目前保存为补丁，不能将作品仓 PR 视为公共仓改动已获合入。详见 [提交状态](docs/SUBMISSION.md)。
