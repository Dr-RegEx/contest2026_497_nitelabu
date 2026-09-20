# Korvo OV3660 摄像头实时采集上屏

## 实测范围与结论

Korvo-1 + OV3660 + SUB3 实物采集显示通过。ID=0x3660，640×480 RGB565 居中显示于 800×480 framebuffer；颜色修复后 600/600 PASS，135.35 秒，复核 135.33 秒（约 4.43fps），用户确认景物变化、红绿蓝颜色及画面完整性“完全正常”。不重启重开 20→600 帧另有修复前颜色版本的记录，不冒称最终确认时未重启。不包含 OV2640/JPEG、动态帧率、SMP 或其它板型。

本次上传整理不重新操作板卡。历史固件、配置及日志保存在本目录；新编译镜像是复现候选，编译成功不替代新实板结果。

## 本目录内容

`src/` 是实际源码，`config/` 包含本配置全部继承文件，`evidence/` 保存测试原始记录（凭据脱敏见根目录 REDACTIONS.json），`case.json` 记录文件来源。没有符号链接。`firmware/` 为历史通过镜像，仅供匹配硬件复核；刷写前核对证据中的 SHA256。

## 从完整工程开始

Ubuntu 22.04/Linux x86_64，准备 GitHub、Gitee、PyPI 网络访问。在新的目录执行；补丁应用只对干净固定基线执行一次。

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

S31 HAL 与工具链按锁定版本准备。不要混用旧 Make 生成的源树 `.config`/`include/nuttx/config.h`。

## 构建

在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" korvo-camera-preview 8
```

输出：`out/esp32s31-korvo-camera-preview/`。修改例程源码时，将 `src/` 中相同相对路径的文件同步到工作区后重新构建；`src/esp-hal-3rdparty/` 对应 `.s31-deps/esp-hal-3rdparty/`。配置 `config/` 对应工作区 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/`。不要仅修改副本却构建旧代码。

## 刷写与终端

确认实际 S31 串口及硬件，进入下载模式。首次刷写前备份 Flash，保存到不覆盖旧备份的位置；不执行全片擦除。以下在工作区根目录操作：

```bash
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  read-flash 0 0x1000000 original-flash.bin
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash 0x2000 out/esp32s31-korvo-camera-preview/nuttx.bin
.s31-deps/venv/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200
```

本配置为 FLAT/Simple Boot，仅烧写单镜像。 串口一次只由一个程序占用，运行主机脚本前退出终端。

## 运行步骤

硬件必须匹配 Korvo-1 + OV3660 + SUB3。使用原配排线，断电检查方向；SCCB 为 I2C0 SDA=0、SCL=1、地址 0x3c，XCLK=16MHz。其余 DVP/LCD 引脚由本目录驱动和配置固定，不沿用 Function 板引脚。启动先读传感器 ID，必须为 0x3660，然后注册 `/dev/video0` 与 `/dev/fb0`。

NSH 执行：

```text
s31preview --frames 20
s31preview --frames 600
```

两条顺序执行，中间不复位，用于核验启停重开。正式保存完整 600 帧日志，应出现 `completed=600/600 result=PASS`，每帧 `bytesused=614400`、`update=OK`；历史耗时约 135 秒只是实测参考，不是 30fps 承诺。

对着真实景物移动或遮挡镜头，确认画面相应变化，再观察红、绿、蓝物体与画面完整性。输出是摄像头原始 RGB565 经字节序转换后居中上屏，不是测试图、JPEG 或录像文件。命令退出后可再次运行。

## 历史证据

- `evidence/README.md`
- `evidence/probe/receipt.json`
- `evidence/preview/receipt.json`
- `evidence/preview-stage/receipt.json`
- `evidence/preview-dma-stage/receipt.json`
- `evidence/preview-irq-stage/receipt.json`
- `evidence/preview-native-irq/receipt.json`
- `evidence/preview-clean/receipt.json`
- `evidence/preview-restart/receipt.json`
- `evidence/preview-restart-order/receipt.json`
- `evidence/preview-color/receipt.json`
- `evidence/preview-color/result.json`
- `evidence/preview-color-confirm/uart.log`
- `evidence/preview-color-confirm/result.json`

各文件的来源与当前副本哈希见本目录 `case.json`。历史记录中较早的“待确认”或失败状态保留，当前结论以本页和最终复核记录为准。原始绝对路径和证据间链接只作历史定位，不是运行依赖。
