# 系统RAM资源占用统计

## 已通过范围

- 1.2.3：🟢 已通过；736：两核资源统计；共享内存解释见xts736-footprint.md

执行 free、ps，并区分 CPU0/CPU1 与共享堆，不能把共享内存重复相加。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/nshlib/nsh_proccmds.c](../_sources/apps/nshlib/nsh_proccmds.c)：原工程 `apps/nshlib/nsh_proccmds.c`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-xts-io](../../configs/demo-rmt-xts-io/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-xts-io 8
```

产物位于 `out/esp32s31-demo-rmt-xts-io/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、在nsh中输入 Free，若存在多核需在每个核都输入
2、等待执行结果

**预期结果：**

1、统计当前系统总体RAM使用情况

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 471 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [xts736-footprint.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/xts736-footprint.md)
- [logs/xts736-footprint.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts736-footprint.log)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
