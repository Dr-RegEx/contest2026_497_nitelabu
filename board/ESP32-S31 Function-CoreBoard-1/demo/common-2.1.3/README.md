# Cold Boot启动时间测试

## 已通过范围

- 2.1.3：🟢 已通过；1604：10次真实USB上下电，平均0.265831秒

必须真实 USB 断电/上电 10 次；计时口径为 ROM 日志到 NSH，RESET 保持时间不计入。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/system/nsh/nsh_main.c](../_sources/apps/system/nsh/nsh_main.c)：原工程 `apps/system/nsh/nsh_main.c`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-netapps-competition](../../configs/demo-rmt-netapps-competition/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-netapps-competition 8
```

产物位于 `out/esp32s31-demo-rmt-netapps-competition/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、将设备上下电cold boot 10次，统计minicom串口工具时间戳开始，到系统启动完成对应的的关键字‘NuttShell (NSH)’的平均启动时长
 注：
1）打开minicom时间戳打印：ctrl +A +Z +N
2）整理log竖排打印：ctrl +A +U

**预期结果：**

1、设备cold boot 10次平均时长不超过4秒

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 1020 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [cold1604-round06/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/cold1604-round06/result.json)
- [cold1604-round05/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/cold1604-round05/result.json)
- [cold1604-round10/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/cold1604-round10/result.json)
- [cold1604-round08/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/cold1604-round08/result.json)
- [cold1604-round09/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/cold1604-round09/result.json)
- [cold1604-round07/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/cold1604-round07/result.json)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
