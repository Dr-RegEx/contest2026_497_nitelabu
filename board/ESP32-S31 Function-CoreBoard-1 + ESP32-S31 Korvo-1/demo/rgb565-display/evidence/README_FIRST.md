# 把原生 openvela RGB565 变更交给队友

本包对应 2026-09-19 已在 ESP32-S31-Korvo-1 + SUB3 上运行并目视通过的版本。
用户确认：“已正常循环，颜色和位置正常”。重新上电自动运行。

## 包里有什么

- `patches/nuttx-rgb565.patch`：LCD framebuffer 驱动、Korvo 专用配置、4 个原有文件中的接入增量，共 6 个文件。
- `patches/apps-rgb565.patch`：`examples/s31rgb565/` 应用，共 8 个新文件。
- `overlay/tools/openvela-rgb565-demo/`：构建、双备份烧录、串口采集脚本及布局预览。
- `firmware/nuttx.bin`、`firmware/tested.config`：实板通过的固件和配置。
- `evidence/`：原生 NuttX 启动、完整循环、自检、烧录结果及用户目视确认记录。
- `BASELINE.json`：仓库版本参考及 4 个原有文件的前后哈希。
- `PATCH-CHECK.json`：补丁检查、应用、结果逐字匹配和反向检查结果。
- `SHA256SUMS`：包内文件校验表。

补丁只包含本次 RGB565 代码。没有打包其它适配改动、独立 ESP-IDF 实验、原板 Flash 备份或工具链。

## 队友接收步骤

前提：队友已有你们现有的 ESP32-S31 openvela 适配工程及其依赖。
本包是增量，不能替代完整 S31 移植，也不能仅凭 BASELINE.json 的 HEAD 在干净上游上重现。
现有 S31 的启动、PSRAM 用户堆、GPIO、GDMA/cache HAL 接入均为前置依赖。

先解压并在包目录验证：

```bash
sha256sum -c SHA256SUMS
```

设置两个绝对路径，指向解压目录和队友已有的工程根目录：

```bash
RGB565_PACKAGE=/绝对路径/openvela-rgb565-handoff-20260920
RGB565_PROJECT=/绝对路径/esp32s31-team
```

先分别执行下面两条检查，两条都成功后再应用：

```bash
git -C "$RGB565_PROJECT/openvela-dev/nuttx" apply --check "$RGB565_PACKAGE/patches/nuttx-rgb565.patch"
git -C "$RGB565_PROJECT/openvela-dev/apps" apply --check "$RGB565_PACKAGE/patches/apps-rgb565.patch"
```

```bash
git -C "$RGB565_PROJECT/openvela-dev/nuttx" apply "$RGB565_PACKAGE/patches/nuttx-rgb565.patch"
git -C "$RGB565_PROJECT/openvela-dev/apps" apply "$RGB565_PACKAGE/patches/apps-rgb565.patch"
mkdir -p "$RGB565_PROJECT/tools"
cp -an "$RGB565_PACKAGE/overlay/tools/openvela-rgb565-demo" "$RGB565_PROJECT/tools/"
```

若 `--check` 报错，应先比较队友的适配基线与补丁上下文；不要直接强行覆盖现有文件。
若目标已有同名工具目录，`cp -an` 保留已有文件，需比较版本后合并。
交付包里的代码补丁已在临时目录应用并逐文件核对；未在队友的工程上执行。

## 构建与板上复现

本包的 build.py 对应当前工程目录布局，需要这些现有目录：

- `openvela-dev/{nuttx,apps,prebuilts}` 及现有工程其它依赖仓库。
- `s31-reference/deps/esp-hal-3rdparty`，锁定 HEAD `290edc31b50decca660c1a11ce3506fd9b2e1e27`。
- `s31-reference/tmp/esp-idf-clean`：本地芯片工具依赖；构建入口仍为 NuttX CMake。
- `s31-reference/.venv-nuttx/bin/olddefconfig`。
- `host-tools/espressif`：项目已有 Python 环境和 `riscv32-esp-elf/esp-15.2.0_20251204` 工具链。

若队友工具链布局不同，按本地目录调整 build.py 的路径。

```bash
cd "$RGB565_PROJECT"
python3 tools/openvela-rgb565-demo/build.py --jobs 8
host-tools/espressif/python_env/idf6.1_py3.10_env/bin/python tools/openvela-rgb565-demo/flash.py --port /dev/ttyUSB0
host-tools/espressif/python_env/idf6.1_py3.10_env/bin/python tools/openvela-rgb565-demo/capture.py --port /dev/ttyUSB0 --seconds 40
```

flash.py 烧录的是队友重建的输出，要求构建回执哈希匹配，自动对其开发板受影响区域做双备份并核验。
`firmware/nuttx.bin` 用于对照本次已经验证的版本；它是 Simple Boot 镜像，写入位置为 `0x2000`。
本包没有操作队友开发板。固件只能在匹配的 Korvo-1 + SUB3 硬件和 Flash 布局上使用。

## 测试内容与范围

屏幕：800×480 RGB565、小端、16 bpp、stride 1600、PCLK 18 MHz。
每 30 秒循环：测试仪表板 16 秒；红、绿、蓝、白、黑各 2 秒；棋盘格 4 秒。
测试图包含八色条、颜色值、R5/G6/B5 与灰阶渐变和动态颜色区域。

原生应用通过 `/dev/fb0` 的 ioctl/mmap/FBIO_UPDATE 绘图。驱动使用 PSRAM 双扫描缓冲区，
轮询真实 AXI GDMA link-switch 完成位，等待时让出 CPU，设置 1 秒截止时间。
该版本规避了 vendor/NuttX 中断管理未衔接导致的首帧停住问题；并未修复公共 IRQ 适配。
验证范围是单核 FLAT、80 MHz 八线 PSRAM、独立保持唤醒的演示，不含 SMP/MMU 或多外设整合。

证据见 [串口日志](evidence/uart.log)、[运行判定](evidence/runtime-result.json)、
[烧录摘要](evidence/flash-summary.json)、[用户目视确认](evidence/user-observation.json)。
40 秒记录包含全部七种模式、回到测试图、至少 100 帧完成，无运行错误。
自动日志中的 optical=unverified 是串口采集的边界，另附的用户确认给出了目视结果。
布局预览不是实物照片。

## 走 Git/PR 时

在团队已保存现有 S31 适配基线的分支上分别应用 NuttX/apps 补丁，审查后分别提交；
测试工具放入你们的工程集成仓库。不要在当前混有其它未提交适配工作的目录里直接 `git add .`。
本次交付只生成本地文件，没有创建提交、推送远端或向队友发送消息。
