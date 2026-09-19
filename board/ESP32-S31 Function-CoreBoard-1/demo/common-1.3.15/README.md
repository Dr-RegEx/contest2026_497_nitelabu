# Watchdog测试

## 已通过范围

- 1.3.15：🟢 已通过；1016：原始模式0→1→2→3及真实看门狗复位通过

按模式 0→1→2→3 执行，模式对应复位属于预期行为；保持串口日志跨复位采集。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/testing/drivers/drivertest/drivertest_watchdog.c](../_sources/apps/testing/drivers/drivertest/drivertest_watchdog.c)：原工程 `apps/testing/drivers/drivertest/drivertest_watchdog.c`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[xts-flat-wdt](../../configs/xts-flat-wdt/defconfig)。
- 镜像类型：FLAT（仅 nuttx.bin）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-wdt 8
```

产物位于 `out/esp32s31-xts-flat-wdt/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、在nsh中依次输入如下命令：
cmocka_driver_watchdog -r 0 //测试到达timeout后看门狗是否生效
cmocka_driver_watchdog -r 1 //测试打断critical_section，在关中断的情况下不喂狗也可以进入wdt中断。
cmocka_driver_watchdog -r 2 //测试开中断后死循环看门狗是否生效
cmocka_driver_watchdog -r 3 //测试正常喂狗，结果输出PASS
说明：执行命令cmocka_driver_watchdog，通过-r传入参数，参数为0-3，分别进入4个不同的case，所以watchdog的测试需要执行四次，参数从0到3依次执行（需按顺序执行）

**预期结果：**

1、观察-r参数为 0/1/2测试咬狗状态能否主动触发asser和打印堆栈信息并重启，并且重启原因是BOARDIOC_RESETCAUSE_SYS_RWDT，-r 参数为3时正常喂狗输出PASS

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 883 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
cmocka_driver_watchdog -r 0
cmocka_driver_watchdog -r 1
cmocka_driver_watchdog -r 2
cmocka_driver_watchdog -r 3
```

来源：[xts1016-original-watchdog.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1016-original-watchdog.log)。

## 历史通过依据

- [checkpoint1016-watchdog-target/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1016-watchdog-target/result.json)
- [checkpoint1016-watchdog-target/xts-current-status.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1016-watchdog-target/xts-current-status.md)
- [checkpoint1016-watchdog-target/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1016-watchdog-target/SHA256SUMS)
- [logs/xts1016-original-watchdog.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1016-original-watchdog.log)
- [checkpoint1016-watchdog-target/logs/xts1016-original-watchdog.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1016-watchdog-target/logs/xts1016-original-watchdog.log)
- [checkpoint1016-watchdog-target/logs/xts1008-original-watchdog.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1016-watchdog-target/logs/xts1008-original-watchdog.log)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
