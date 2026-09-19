# Uart串口功能测试

## 已通过范围

- 1.3.10：🟢 已通过；744：UART三模式及10次突发采样通过

历史命令 `cmocka_driver_uart -d /dev/ttyS0 -n0`、`-n1`、`-n2`，主机按原文发送规定字符与 10 次突发数据，测试时独占串口。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/testing/drivers/drivertest/drivertest_uart.c](../_sources/apps/testing/drivers/drivertest/drivertest_uart.c)：原工程 `apps/testing/drivers/drivertest/drivertest_uart.c`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-xts-drivers](../../configs/demo-rmt-xts-drivers/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-xts-drivers 8
```

产物位于 `out/esp32s31-demo-rmt-xts-drivers/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

优先用手动测试，脚本存在问题需要修正
| 手动测试
1、在nsh中输入 ls /dev，以查询设备名称，如ttyS0
2、在nsh中输入：cmocka_driver_uart -d /dev/ttyS0
3、在串口粘贴复制如下内容并回车
0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ,./<>?;':"[]{}\|!@/#$%^&/*()-+_=
4、输入0 然后回车
5、输入/#
6、查看是否测试PASS
| 脚本测试
1、在插入了USB转UART模块的电脑上，在/dev目录中找到对应的tty设备：ls /dev/ttyUSB/* ，/*为数字
2、在电脑终端执行：
sudo python3 testing/drivertest/test_content_gen.py /dev/ttyUSB/*（/*根据实际情况填写）
3、在nsh中输入 ls /dev，以查询设备名称，如ttyS4
4、继续在nsh中输入 cmocka_driver_uart -d /dev/ttyS4，等待执行结果

**预期结果：**

测试结果PASS，无异常

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 719 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
cmocka_driver_uart -d /dev/ttyS0 -n 0
cmocka_driver_uart -d /dev/ttyS0 -n 1
cmocka_driver_uart -d /dev/ttyS0 -n 2
```

来源：[xts744-uart.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts744-uart.log)。

## 历史通过依据

- [checkpoint744-xts.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint744-xts.md)
- [logs/xts744-uart.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts744-uart.log)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
