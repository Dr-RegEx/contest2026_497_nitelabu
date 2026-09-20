# Watchdog测试

## 已通过范围

- 1.3.15：🟢 已通过；1016：原始模式0→1→2→3及真实看门狗复位通过

按模式 0→1→2→3 执行，模式对应复位属于预期行为；保持串口日志跨复位采集。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/testing/drivers/drivertest/drivertest_watchdog.c](src/drivertest_watchdog.c)：原工程 `apps/testing/drivers/drivertest/drivertest_watchdog.c`。

`src/` 直接保存本例程的实际源码文件，`config/` 保存构建配置及其全部继承配置，`licenses/` 保留相关许可证。编译依赖完整 openvela 工作区；下面列出了从空目录拉取到运行的步骤。若修改本地例程源码，按下方命令同步到编译树后重新构建。

- 构建配置：[xts-flat-wdt](config/xts-flat-wdt/defconfig)。
- 镜像类型：FLAT（仅 nuttx.bin）。

## 环境与完整工程准备

环境为 Linux x86_64 / Ubuntu 22.04，需要 GitHub、Gitee 和 PyPI 网络访问。已完成以下初始化的队员可直接构建；补丁应用仅在干净的固定版本工作区执行一次。

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

跳过的 LFS 文件属于其他板型；S31 HAL 与工具链由依赖脚本按固定版本准备。不要混入 Make 生成的 `nuttx/.config` 或 `nuttx/include/nuttx/config.h`。

## 构建

在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-wdt 8
```

产物位于 `out/esp32s31-xts-flat-wdt/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。


### 修改例程源码后同步

首次复现可直接使用上面的构建命令。修改本目录 `src/` 后，在工作区根目录执行以下命令将修改同步到对应编译位置，再执行同一构建命令；这些命令会覆盖对应源文件，请先保存自己的修改。

```bash
cp -a "contest2026_497_nitelabu/board/ESP32-S31 Function-CoreBoard-1/demo/common-1.3.15/src/drivertest_watchdog.c" "apps/testing/drivers/drivertest/drivertest_watchdog.c"
```

`config/` 是本例程配置的实体快照；如需修改配置，请将对应文件同步到工作区 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/` 的相同相对位置。

## 刷写与终端

确认 S31 串口（不要选到 C6），让 S31 进入下载模式。以下命令在工作区根目录执行，串口按实际修改。首次刷写前备份 Flash，并保存到不会被覆盖的位置。

```bash
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  read-flash 0 0x1000000 original-flash.bin
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash 0x2000 out/esp32s31-xts-flat-wdt/nuttx.bin
.s31-deps/venv/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200
```

复位进入 `nsh>` 后执行本页步骤。本配置为 FLAT，仅写入 `nuttx.bin`。不执行全片擦除；涉及文件系统、掉电或介质测试时，还须遵守下面该用例的专用分区要求。主机自动脚本使用串口前先退出串口终端。

## 运行步骤与预期结果

1、在nsh中依次输入如下命令：
cmocka_driver_watchdog -r 0 //测试到达timeout后看门狗是否生效
cmocka_driver_watchdog -r 1 //测试打断critical_section，在关中断的情况下不喂狗也可以进入wdt中断。
cmocka_driver_watchdog -r 2 //测试开中断后死循环看门狗是否生效
cmocka_driver_watchdog -r 3 //测试正常喂狗，结果输出PASS
说明：执行命令cmocka_driver_watchdog，通过-r传入参数，参数为0-3，分别进入4个不同的case，所以watchdog的测试需要执行四次，参数从0到3依次执行（需按顺序执行）

**预期结果：**

1、观察-r参数为 0/1/2测试咬狗状态能否主动触发asser和打印堆栈信息并重启，并且重启原因是BOARDIOC_RESETCAUSE_SYS_RWDT，-r 参数为3时正常喂狗输出PASS

完整原始用例与前提配置已附在本页末尾。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
cmocka_driver_watchdog -r 0
cmocka_driver_watchdog -r 1
cmocka_driver_watchdog -r 2
cmocka_driver_watchdog -r 3
```

来源：[xts1016-original-watchdog.log](evidence/workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1016-original-watchdog.log)。

## 历史通过依据

- [checkpoint1016-watchdog-target/result.json](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1016-watchdog-target/result.json)
- [checkpoint1016-watchdog-target/xts-current-status.md](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1016-watchdog-target/xts-current-status.md)
- [checkpoint1016-watchdog-target/SHA256SUMS](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1016-watchdog-target/SHA256SUMS)
- [logs/xts1016-original-watchdog.log](evidence/workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1016-original-watchdog.log)
- [checkpoint1016-watchdog-target/logs/xts1016-original-watchdog.log](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1016-watchdog-target/logs/xts1016-original-watchdog.log)
- [checkpoint1016-watchdog-target/logs/xts1008-original-watchdog.log](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1016-watchdog-target/logs/xts1008-original-watchdog.log)
- [xts-current-status.md](evidence/docs/acceptance/xts-current-status.md)

核验集（历史清单已移除） 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。

## 原始用例全文（历史标准）

以下保留原始用例文字；设备映射与本例程通过边界以上文为准。

##### 1.3.15 Watchdog测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.3 驱动BSP

**前提条件：**

1. 打开如下配置：

```
CONFIG_WATCHDOG=y
CONFIG_BOARDCTL_RESET_CAUSE=y
CONFIG_TESTING_DRIVER_TEST=y
CONFIG_CMOCKA=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口
注：芯片厂商初始化wdt时需要在wdt中断里面主动调用panic，并且提高watchdog中断优先级，保证在watchdog中断可以打断critical_section，在关中断的情况下不喂狗也可以进入wdt中断。

**步骤：**

1、在nsh中依次输入如下命令：
cmocka_driver_watchdog -r 0 //测试到达timeout后看门狗是否生效
cmocka_driver_watchdog -r 1 //测试打断critical_section，在关中断的情况下不喂狗也可以进入wdt中断。
cmocka_driver_watchdog -r 2 //测试开中断后死循环看门狗是否生效
cmocka_driver_watchdog -r 3 //测试正常喂狗，结果输出PASS
说明：执行命令cmocka_driver_watchdog，通过-r传入参数，参数为0-3，分别进入4个不同的case，所以watchdog的测试需要执行四次，参数从0到3依次执行（需按顺序执行）

**预期结果：**

1、观察-r参数为 0/1/2测试咬狗状态能否主动触发asser和打印堆栈信息并重启，并且重启原因是BOARDIOC_RESETCAUSE_SYS_RWDT，-r 参数为3时正常喂狗输出PASS

---
