# 12h待机稳定性测试

## 已通过范围

- 3.1.1：🟢 已通过；864：未配网12小时，资源无增长、无复位/异常

原始 12h 未配网待机，保留供电、UART 和镜像，定期记录资源。当前只整理例程，不启动或重跑长测。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/system/resmonitor](src/resmonitor)：原工程 `apps/system/resmonitor`。

`src/` 直接保存本例程的实际源码文件，`config/` 保存构建配置及其全部继承配置，`licenses/` 保留相关许可证。编译依赖完整 openvela 工作区；下面列出了从空目录拉取到运行的步骤。若修改本地例程源码，按下方命令同步到编译树后重新构建。

- 构建配置：[demo-rmt-xts-standby](config/demo-rmt-xts-standby/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。

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
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-xts-standby 8
```

产物位于 `out/esp32s31-demo-rmt-xts-standby/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。


### 修改例程源码后同步

首次复现可直接使用上面的构建命令。修改本目录 `src/` 后，在工作区根目录执行以下命令将修改同步到对应编译位置，再执行同一构建命令；这些命令会覆盖对应源文件，请先保存自己的修改。

```bash
cp -a "contest2026_497_nitelabu/board/ESP32-S31 Function-CoreBoard-1 + ESP32-S31 Korvo-1/demo/common-3.1.1/src/resmonitor/." "apps/system/resmonitor/"
```

`config/` 是本例程配置的实体快照；如需修改配置，请将对应文件同步到工作区 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/` 的相同相对位置。

## 刷写与终端

确认 S31 串口（不要选到 C6），让 S31 进入下载模式。以下命令在工作区根目录执行，串口按实际修改。首次刷写前备份 Flash，并保存到不会被覆盖的位置。

```bash
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  read-flash 0 0x1000000 original-flash.bin
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash 0x2000 out/esp32s31-demo-rmt-xts-standby/nuttx.bin \
  0x200000 out/esp32s31-demo-rmt-xts-standby/appfs.img
.s31-deps/venv/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200
```

复位进入 `nsh>` 后执行本页步骤。本配置为 Kernel，`nuttx.bin` 和 `appfs.img` 必须来自同一次构建的同一输出目录，不能混配；可写数据分区从 `0x500000` 开始。不执行全片擦除；涉及文件系统、掉电或介质测试时，还须遵守下面该用例的专用分区要求。主机自动脚本使用串口前先退出串口终端。

## 运行步骤与预期结果

1、设备静置12h，保存串口log

**预期结果：**

1、系统无报错、crash、重启等异常情况

完整原始用例与前提配置已附在本页末尾。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [checkpoint864-standby12h/result.json](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint864-standby12h/result.json)
- [clock864-host-audit.md](evidence/workspace/evidence/backups-2026-09-10-scan-stress/clock864-host-audit.md)
- [clock864-acceptance-989.md](evidence/workspace/evidence/backups-2026-09-10-scan-stress/clock864-acceptance-989.md)
- [checkpoint864-kasan-longrun.md](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint864-kasan-longrun.md)
- [clock864-final-assessment-988.md](evidence/workspace/evidence/backups-2026-09-10-scan-stress/clock864-final-assessment-988.md)
- [checkpoint864-standby12h/README.md](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint864-standby12h/README.md)
- [xts-current-status.md](evidence/docs/acceptance/xts-current-status.md)

核验集（历史清单已移除） 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。

## 原始用例全文（历史标准）

以下保留原始用例文字；设备映射与本例程通过边界以上文为准。

##### 3.1.1 12h待机稳定性测试

**测试目的：** 验证设备未配网状态下，长时间待机，系统无异常

**前提条件：**

1、将设备上电
2、编译开启内存监控kasan和show_info测试工具的版本
开启show_info
LOW_RESOURCE_TEST=y
启用kasan

```
CONFIG_MM_KASAN=y
```

**步骤：**

1、设备静置12h，保存串口log

**预期结果：**

1、系统无报错、crash、重启等异常情况

---
