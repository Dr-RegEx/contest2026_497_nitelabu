# I2C 通用主从通信（S31 ↔ C6）

## 已通过范围

- 1.3.7：🟢 通用通信功能已通过；按用户指定S31主机↔C6从机验收：[I2C](../../../../docs/acceptance/i2c-generic-echo-result.md)100/400kHz、32/32组回传一致；[SPI](../../../../docs/acceptance/spi-c6-result.md)100kHz/1MHz、C6接收14/14帧、S31回传13/13帧一致。独立测试配置，不采用原始传感器用例

采用用户指定的 S31 主机 ↔ C6 从机通用通信验收；不声称原始传感器用例通过。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/system/i2c](../_sources/apps/system/i2c)：原工程 `apps/system/i2c`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[xts-flat-i2c-c6](../../configs/xts-flat-i2c-c6/defconfig)。
- 镜像类型：FLAT（仅 nuttx.bin）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-i2c-c6 8
```

产物位于 `out/esp32s31-xts-flat-i2c-c6/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

采用用户指定的 S31 主机 ↔ C6 从机通用通信验收；不声称原始传感器用例通过。

### 接线与 C6 夹具

断电接线，S31→C6：45→4（SCL）、46→5（SDA）、GND→GND。双方分别 USB 供电，不互连 5V/3V3。
`c6/` 链接到仓内真实夹具工程，含源码与主机验证脚本。
C6 使用 ESP-IDF `14f663f003eb8fd9a688c301a412a9540d29dacf`（6.1 开发版），按 ESP-IDF 安装该版本并激活自己的 export.sh，无需原作者的本机目录。

```bash
# 在已激活该 ESP-IDF 版本的终端，进入复刻仓根目录
cd workspace/tools/esp32c6-i2c-slave
idf.py build
idf.py -p /dev/ttyACM0 flash
```

确认两个串口对应的芯片；以上 C6 刷写端口和下方验证端口须按实际修改。S31 使用上文 FLAT 镜像，刷写方法见根 README。
在 openvela 工作区根目录、其他串口终端退出后执行：

```bash
.s31-deps/venv/bin/python contest2026_497_nitelabu/workspace/tools/esp32c6-i2c-slave/verify_echo.py --master /dev/ttyUSB0 --slave /dev/ttyACM0 --log /tmp/i2c-demo-new.log
```

日志文件必须尚不存在。I2C 地址 0x68，100/400kHz、1/2 字节共 32/32 组一致。

## 历史通过依据

- [i2c-generic-echo-result.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/i2c-generic-echo-result.md)
- [i2c-generic-echo-validation.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/i2c-generic-echo-validation.log)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
