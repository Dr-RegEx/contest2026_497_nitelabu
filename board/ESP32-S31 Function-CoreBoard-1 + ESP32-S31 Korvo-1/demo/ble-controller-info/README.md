# BLE 控制器版本与 LE 特性实板读取

## 实测范围与结论

1747 Kernel/AppFS 配对镜像实际读取控制器 HCI=6.0(0x0e)、rev=0x0000、mfr=0x02e5、LEfeat=ff79ffff901b0000，enable/disable 正常。数据来自 HCI 0x1001/0x2003 的控制器回包，纠正旧库名推断；不证明全部 BLE 5.4 能力、认证、LE Audio、Mesh 或全部 xTS 互通通过。

本次上传整理不重新操作板卡。历史固件、配置及日志保存在本目录；新编译镜像是复现候选，编译成功不替代新实板结果。

## 本目录内容

`src/` 是实际源码，`config/` 包含本配置全部继承文件，`evidence/` 保存测试原始记录（凭据脱敏见根目录 REDACTIONS.json），`case.json` 记录文件来源。没有符号链接。

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
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-bttool-kernel 8
```

输出：`out/esp32s31-demo-rmt-bttool-kernel/`。修改例程源码时，将 `src/` 中相同相对路径的文件同步到工作区后重新构建；`src/esp-hal-3rdparty/` 对应 `.s31-deps/esp-hal-3rdparty/`。配置 `config/` 对应工作区 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/`。不要仅修改副本却构建旧代码。

## 刷写与终端

确认实际 S31 串口及硬件，进入下载模式。首次刷写前备份 Flash，保存到不覆盖旧备份的位置；不执行全片擦除。以下在工作区根目录操作：

```bash
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  read-flash 0 0x1000000 original-flash.bin
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash 0x2000 out/esp32s31-demo-rmt-bttool-kernel/nuttx.bin \
  0x200000 out/esp32s31-demo-rmt-bttool-kernel/appfs.img
.s31-deps/venv/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200
```

本配置为 Kernel，内核与 AppFS 必须来自同一次构建；不要混用历史镜像。 串口一次只由一个程序占用，运行主机脚本前退出终端。

## 运行步骤

NSH 中确认 `/dev/ttyHCI0` 和可写的 `/data/misc/bt`。若 `/data` 尚未挂载，可为本次临时测试挂载 tmpfs；已有真实数据分区时不要覆盖挂载或格式化。

```text
mkdir -p /data
mount -t tmpfs /data
mkdir -p /data/misc/bt
bttool
enable
state
```

上面的 mount 仅适用于未挂载 `/data` 的情况。保存控制器版本/LE 特性日志及初始化结果；历史 state=2。然后执行：

```text
disable
state
quit
```

应回到 state=0 并退出 NSH。保存实际回包值，不把常量、库文件名或 Host 版本当作控制器实测，也不据此修改全部 BLE 用例状态。

## 历史证据

- `evidence/review.md`
- `evidence/uart.log`
- `evidence/result.json`

各文件的来源与当前副本哈希见本目录 `case.json`。历史记录中较早的“待确认”或失败状态保留，当前结论以本页和最终复核记录为准。原始绝对路径和证据间链接只作历史定位，不是运行依赖。
