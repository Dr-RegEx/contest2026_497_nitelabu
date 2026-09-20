# GPIO47/48 实物回环与上升沿中断

## 实测范围与结论

GPIO47↔48 杜邦线实测：低/高电平两组各 4/4 PASS，回环 0/1 一致，上升沿 IRQ count=1。两组在同一 Kernel/AppFS 配对镜像的分别启动中完成。连续第二次启动仅回显、20 秒超时的问题仍保留，不宣称重复启动稳定性已修复。

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
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-gpio-loopback 8
```

输出：`out/esp32s31-demo-gpio-loopback/`。修改例程源码时，将 `src/` 中相同相对路径的文件同步到工作区后重新构建；`src/esp-hal-3rdparty/` 对应 `.s31-deps/esp-hal-3rdparty/`。配置 `config/` 对应工作区 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/`。不要仅修改副本却构建旧代码。

## 刷写与终端

确认实际 S31 串口及硬件，进入下载模式。首次刷写前备份 Flash，保存到不覆盖旧备份的位置；不执行全片擦除。以下在工作区根目录操作：

```bash
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  read-flash 0 0x1000000 original-flash.bin
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash 0x2000 out/esp32s31-demo-gpio-loopback/nuttx.bin \
  0x200000 out/esp32s31-demo-gpio-loopback/appfs.img
.s31-deps/venv/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200
```

本配置为 Kernel，内核与 AppFS 必须来自同一次构建；不要混用历史镜像。 串口一次只由一个程序占用，运行主机脚本前退出终端。

## 运行步骤

硬件为 Function-CoreBoard-1。断电后用一根杜邦线连接 GPIO47 与 GPIO48；GPIO48 为输出 `/dev/gpio1`，GPIO47 为输入 `/dev/gpio0`，不要接电源脚。

第一次启动，在 NSH 执行：

```text
cmocka_driver_gpio -i /dev/gpio0 -o /dev/gpio1 -p 0 -l -r 1
```

记录 4/4 PASS，并确认实际 IRQ count=1。保存日志后复位，第二次启动执行：

```text
cmocka_driver_gpio -i /dev/gpio0 -o /dev/gpio1 -p 1 -l -r 1
```

同样要求 4/4 PASS、输入输出高电平一致且 IRQ count=1。原标准中的 `-a/-b` 与当前解析器不一致，这里使用有效的 `-i/-o`；不修改原断言。不要把超时或仅有命令回显当成通过。

## 历史证据

- `evidence/receipt.json`
- `evidence/review.md`
- `evidence/kernel/level0.log`
- `evidence/kernel/receipt.json`
- `evidence/kernel/high-retry/level1.log`
- `evidence/current/receipt.json`

各文件的来源与当前副本哈希见本目录 `case.json`。历史记录中较早的“待确认”或失败状态保留，当前结论以本页和最终复核记录为准。原始绝对路径和证据间链接只作历史定位，不是运行依赖。
