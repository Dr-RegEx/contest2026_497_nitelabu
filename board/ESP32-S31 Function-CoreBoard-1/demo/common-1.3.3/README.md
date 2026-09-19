# RAM读写性能测试

## 已通过范围

- 1.3.3：🟢 已通过；705/706/707：字/半字/字节模式通过

历史参数分别为 `ramtest -w -s 209256`、`ramtest -h -s 209256`、`ramtest -b -s 209256`。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/testing/mm/ramtest](../_sources/apps/testing/mm/ramtest)：原工程 `apps/testing/mm/ramtest`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-xts-memory](../../configs/demo-rmt-xts-memory/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-xts-memory 8
```

产物位于 `out/esp32s31-demo-rmt-xts-memory/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、在nsh中输入 free，获取系统当前最大空闲内存块(largest)的size
2、在nsh中输入 ramtest [-w|h|b] -s <size>，等待执行结果

**预期结果：**

测试结果PASS，无异常

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 551 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
ramtest -w -s 209256
ramtest -h -s 209256
ramtest -b -s 209256
```

来源：[xts705-ram-word.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts705-ram-word.log)、[xts706-ram-half.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts706-ram-half.log)、[xts707-ram-byte.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts707-ram-byte.log)。

## 历史通过依据

- [logs/xts705-ram-word.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts705-ram-word.log)
- [logs/xts706-ram-half.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts706-ram-half.log)
- [logs/xts707-ram-byte.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts707-ram-byte.log)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
