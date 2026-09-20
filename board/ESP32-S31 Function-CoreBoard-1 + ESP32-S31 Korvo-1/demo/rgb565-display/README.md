# Korvo + SUB3 RGB565 实屏循环演示

## 实测范围与结论

队友旧固件已通过 RGB565 自检、七种画面循环和至少 100 帧更新，用户确认“已正常循环，颜色和位置正常”。新公开源码另包含后续 LCD/CAM 共享资源修复；新构建结果须与旧实板固件区别记录。本例程是 Korvo-1 + SUB3 独立单核 FLAT 演示，不扩大到 Function 板接线或 SMP/MMU。

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
./contest2026_497_nitelabu/workspace/build.sh "$PWD" korvo-rgb565 8
```

输出：`out/esp32s31-korvo-rgb565/`。修改例程源码时，将 `src/` 中相同相对路径的文件同步到工作区后重新构建；`src/esp-hal-3rdparty/` 对应 `.s31-deps/esp-hal-3rdparty/`。配置 `config/` 对应工作区 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/`。不要仅修改副本却构建旧代码。

## 刷写与终端

确认实际 S31 串口及硬件，进入下载模式。首次刷写前备份 Flash，保存到不覆盖旧备份的位置；不执行全片擦除。以下在工作区根目录操作：

```bash
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  read-flash 0 0x1000000 original-flash.bin
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash 0x2000 out/esp32s31-korvo-rgb565/nuttx.bin
.s31-deps/venv/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200
```

本配置为 FLAT/Simple Boot，仅烧写单镜像。 串口一次只由一个程序占用，运行主机脚本前退出终端。

`preview.png` 仅为软件生成的布局示意，不是开发板实拍或演示视频。

## 运行步骤

连接 Korvo-1 的匹配 SUB3 屏幕，使用原配连接方式。LCD 数据引脚为 8～19、33～36，DISP=38、PCLK=40、DE=43、HSYNC=44、VSYNC=45。不要按 Function 板的 GPIO 测试接线连接屏幕。

该配置上电自动后台运行 `s31rgb565` 并进入 NSH。800×480、RGB565 LE、16bpp、stride 1600、PCLK 18MHz；每轮 30 秒：仪表板 16 秒，红/绿/蓝/白/黑各 2 秒，棋盘格 4 秒。

保留至少 40 秒串口日志，检查 `RGB565_SELFTEST=PASS words=65536`、七种 PATTERN 和 `LCD_FRAMES=100 FRAMEBUFFER_UPDATE=PASS`。另行目视检查颜色、位置、画面完整性和持续循环。串口里的 `optical=unverified` 不能代替目视确认。不要在后台已有演示时再启动第二个绘图进程。

## 历史证据

- `evidence/user-observation.json`
- `evidence/runtime-result.json`
- `evidence/uart.log`

各文件的来源与当前副本哈希见本目录 `case.json`。历史记录中较早的“待确认”或失败状态保留，当前结论以本页和最终复核记录为准。原始绝对路径和证据间链接只作历史定位，不是运行依赖。
