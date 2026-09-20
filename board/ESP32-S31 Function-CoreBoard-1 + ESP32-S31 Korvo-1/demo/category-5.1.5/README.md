# 文件系统多线程文件读写测试vela_fs_stress_multi_thread_file_operate_test

## 已通过范围

- 5.1.5：🟢 已通过；PASS 原始工作量

使用已备份的专用 LittleFS 测试卷和独立工作目录。参数、循环数及挂载方式以原始步骤和对应通过日志为准；禁止把运行目录设为系统 /etc 或其他已有数据目录。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [tests/testcases/vela_fs_test](src/vela_fs_test)：原工程 `tests/testcases/vela_fs_test`。

`src/` 直接保存本例程的实际源码文件，`config/` 保存构建配置及其全部继承配置，`licenses/` 保留相关许可证。编译依赖完整 openvela 工作区；下面列出了从空目录拉取到运行的步骤。若修改本地例程源码，按下方命令同步到编译树后重新构建。

- 构建配置：[xts-flat-category-fs-name](config/xts-flat-category-fs-name/defconfig)。
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
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-category-fs-name 8
```

产物位于 `out/esp32s31-xts-flat-category-fs-name/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。


### 修改例程源码后同步

首次复现可直接使用上面的构建命令。修改本目录 `src/` 后，在工作区根目录执行以下命令将修改同步到对应编译位置，再执行同一构建命令；这些命令会覆盖对应源文件，请先保存自己的修改。

```bash
cp -a "contest2026_497_nitelabu/board/ESP32-S31 Function-CoreBoard-1 + ESP32-S31 Korvo-1/demo/category-5.1.5/src/vela_fs_test/." "tests/testcases/vela_fs_test/"
```

`config/` 是本例程配置的实体快照；如需修改配置，请将对应文件同步到工作区 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/` 的相同相对位置。

## 刷写与终端

确认 S31 串口（不要选到 C6），让 S31 进入下载模式。以下命令在工作区根目录执行，串口按实际修改。首次刷写前备份 Flash，并保存到不会被覆盖的位置。

```bash
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  read-flash 0 0x1000000 original-flash.bin
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash 0x2000 out/esp32s31-xts-flat-category-fs-name/nuttx.bin
.s31-deps/venv/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200
```

复位进入 `nsh>` 后执行本页步骤。本配置为 FLAT，仅写入 `nuttx.bin`。不执行全片擦除；涉及文件系统、掉电或介质测试时，还须遵守下面该用例的专用分区要求。主机自动脚本使用串口前先退出串口终端。

## 运行步骤与预期结果

### S31 测试卷准备

先执行 `mount`、`df` 核对设备和挂载点。本例当前配置注册 `/dev/xtsflash`（1MiB，Flash 区间 0xc00000–0xcfffff）。LARGE 与普通配置使用同一个节点名但映射到不同分区，不能仅看设备名判断旧卷的容量和内容。不要覆盖已有 `/data` 挂载。

确认对应测试区已备份且为可牺牲数据后，首次建卷可使用 LittleFS 的 `forceformat` 挂载选项；已有卷及掉电/crash 后的恢复必须普通挂载，不能 forceformat。需要精确复核历史结果时，使用历史记录对应的配置和分区。

下面为已有卷的挂载示例；仅在 `/data` 未挂载时使用。`sync` 用于要求持久性的测试，历史吞吐或耗时结果以原始记录的挂载选项为准：

```text
mkdir -p /data
mount -t littlefs -o sync /dev/xtsflash /data
```

为本例程新建独立子目录，把原文 `<DIR>` 替换成该路径；原始工作量按下方命令/步骤保留。测试结束保留结果文件和日志，重测不要混入上次遗留数据。

1、在nsh中输入 vela_fs_stress_multi_thread_file_operate_test <DIR>
注：DIR为需要测试的文件系统挂载的目录，可通过df -h来查看，例如/data（默认）， /sdcard， /sst，rpmsgfs(一般非主核挂载)

**预期结果：**

1、输出TEST PASSED

完整原始用例与前提配置已附在本页末尾。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
mkdir /data/s05
vela_fs_stress_multi_thread_file_operate_test -d /data/s05
```

来源：[xts1019-fs-5.1.5.log](evidence/workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1019-fs-5.1.5.log)。

## 历史通过依据

- [checkpoint1019-fs-batch/result.json](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1019-fs-batch/result.json)
- [checkpoint1019-fs-batch/mount/result.json](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1019-fs-batch/mount/result.json)
- [checkpoint1019-fs-batch/xts-category-status.md](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1019-fs-batch/xts-category-status.md)
- [checkpoint1019-fs-batch/SHA256SUMS](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1019-fs-batch/SHA256SUMS)
- [logs/xts1019-fs-5.1.5.log](evidence/workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1019-fs-5.1.5.log)
- [checkpoint1019-fs-batch/xts1537-fs-5.1.8.log](evidence/workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1019-fs-batch/xts1537-fs-5.1.8.log)
- [xts-category-status.md](evidence/docs/acceptance/xts-category-status.md)

核验集（历史清单已移除） 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。

## 原始用例全文（历史标准）

以下保留原始用例文字；设备映射与本例程通过边界以上文为准。

##### 5.1.5 文件系统多线程文件读写测试vela_fs_stress_multi_thread_file_operate_test

**测试目的：** 二、品类自测用例 > 5、性能测试 > 5.1 系统应用 > 文件系统

**前提条件：**

1、打开如下配置：

```
CONFIG_FS_TEST=y
CONFIG_TESTS_TESTCASES=y
CONFIG_TESTING_TESTCASES_STACKSIZE=8192
CONFIG_FS_TEST_FUNCTION=y
CONFIG_FS_TEST_STRESS=y
CONFIG_FS_TEST_STABILITY=y
CONFIG_FS_TEST_POWEROFF=y
CONFIG_ARCH_SETJMP_H=y
CONFIG_PSEUDOFS_SOFTLINKS=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、在nsh中输入 vela_fs_stress_multi_thread_file_operate_test <DIR>
注：DIR为需要测试的文件系统挂载的目录，可通过df -h来查看，例如/data（默认）， /sdcard， /sst，rpmsgfs(一般非主核挂载)

**预期结果：**

1、输出TEST PASSED

---
