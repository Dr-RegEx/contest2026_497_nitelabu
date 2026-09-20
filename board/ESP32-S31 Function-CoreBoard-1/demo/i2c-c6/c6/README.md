# I2C 通用主从通信（S31 ↔ C6）

## 已通过范围

- 1.3.7：🟢 通用通信功能已通过；按用户指定S31主机↔C6从机验收：[I2C](../evidence/docs/acceptance/i2c-generic-echo-result.md)100/400kHz、32/32组回传一致；[SPI](../evidence/docs/acceptance/spi-c6-result.md)100kHz/1MHz、C6接收14/14帧、S31回传13/13帧一致。独立测试配置，不采用原始传感器用例

采用用户指定的 S31 主机 ↔ C6 从机通用通信验收；不声称原始传感器用例通过。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/system/i2c](../src/i2c)：原工程 `apps/system/i2c`。

`src/` 直接保存本例程的实际源码文件，`config/` 保存构建配置及其全部继承配置，`licenses/` 保留相关许可证。编译依赖完整 openvela 工作区；下面列出了从空目录拉取到运行的步骤。若修改本地例程源码，按下方命令同步到编译树后重新构建。

- 构建配置：[xts-flat-i2c-c6](../config/xts-flat-i2c-c6/defconfig)。
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
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-i2c-c6 8
```

产物位于 `out/esp32s31-xts-flat-i2c-c6/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。


### 修改例程源码后同步

首次复现可直接使用上面的构建命令。修改本目录 `src/` 后，在工作区根目录执行以下命令将修改同步到对应编译位置，再执行同一构建命令；这些命令会覆盖对应源文件，请先保存自己的修改。

```bash
cp -a "contest2026_497_nitelabu/board/ESP32-S31 Function-CoreBoard-1/demo/i2c-c6/src/i2c/." "apps/system/i2c/"
```

`config/` 是本例程配置的实体快照；如需修改配置，请将对应文件同步到工作区 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/` 的相同相对位置。

## 刷写与终端

确认 S31 串口（不要选到 C6），让 S31 进入下载模式。以下命令在工作区根目录执行，串口按实际修改。首次刷写前备份 Flash，并保存到不会被覆盖的位置。

```bash
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  read-flash 0 0x1000000 original-flash.bin
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash 0x2000 out/esp32s31-xts-flat-i2c-c6/nuttx.bin
.s31-deps/venv/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200
```

复位进入 `nsh>` 后执行本页步骤。本配置为 FLAT，仅写入 `nuttx.bin`。不执行全片擦除；涉及文件系统、掉电或介质测试时，还须遵守下面该用例的专用分区要求。主机自动脚本使用串口前先退出串口终端。

## 运行步骤与预期结果

采用用户指定的 S31 主机 ↔ C6 从机通用通信验收；不声称原始传感器用例通过。

### 接线与 C6 夹具

断电接线，S31→C6：45→4（SCL）、46→5（SDA）、GND→GND。双方分别 USB 供电，不互连 5V/3V3。
`c6/` 直接包含夹具工程源码与主机验证脚本。
C6 使用 ESP-IDF `14f663f003eb8fd9a688c301a412a9540d29dacf`（6.1 开发版），按 ESP-IDF 安装该版本并激活自己的 export.sh，无需原作者的本机目录。

```bash
# 在已激活该 ESP-IDF 版本的终端，从 openvela 工作区根目录执行
cd "contest2026_497_nitelabu/board/ESP32-S31 Function-CoreBoard-1/demo/i2c-c6/c6"
idf.py build
idf.py -p /dev/ttyACM0 flash
```

确认两个串口对应的芯片；以上 C6 刷写端口和下方验证端口须按实际修改。S31 使用上文 FLAT 镜像，刷写方法见本页“刷写与终端”。
在 openvela 工作区根目录、其他串口终端退出后执行：

```bash
.s31-deps/venv/bin/python "contest2026_497_nitelabu/board/ESP32-S31 Function-CoreBoard-1/demo/i2c-c6/c6/verify_echo.py" --master /dev/ttyUSB0 --slave /dev/ttyACM0 --log /tmp/i2c-demo-new.log
```

日志文件必须尚不存在。I2C 地址 0x68，100/400kHz、1/2 字节共 32/32 组一致。

## 历史通过依据

- [i2c-generic-echo-result.md](../evidence/workspace/evidence/backups-2026-09-10-scan-stress/i2c-generic-echo-result.md)
- [i2c-generic-echo-validation.log](../evidence/workspace/evidence/backups-2026-09-10-scan-stress/i2c-generic-echo-validation.log)
- [xts-current-status.md](../evidence/docs/acceptance/xts-current-status.md)

[当前验收清单](../evidence/docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。

## 夹具补充说明

## 实现要点

C6 收到读请求后，通过事件队列唤醒任务调用 `i2c_slave_write`，提交响应并释放时钟延展。接收数据随事件复制，避免共享缓冲区被下一笔覆盖；检查发送长度与事件丢失。不可仅在启动时预填响应而忽略后续读请求。
