# 时间一致性测试

## 已通过范围

- 1.3.14：🟢 已通过；989项目验收：24h四次记录误差均在2秒内；早期采样间隔及记录缺口保留说明

沿用 24h 四次记录的项目验收口径；原始采样缺口与时间解释见 clock864-acceptance-989.md。新的计时任务需要单独建立连续记录，不能借用旧结果。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/nshlib/nsh_timcmds.c](../_sources/apps/nshlib/nsh_timcmds.c)：原工程 `apps/nshlib/nsh_timcmds.c`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-xts-standby](../../configs/demo-rmt-xts-standby/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-xts-standby 8
```

产物位于 `out/esp32s31-demo-rmt-xts-standby/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1. 打开minicom时间：串口执行ctrl+a，n，回车
2、主核设置与当前PC时间一致，eg: date -s "Nov 11 11:11:00 2022"
3、在nsh中输入 date
4、每隔6h 检查一次date（共检查4次）

**预期结果：**

1、显示当前PC时间
2、设置成功，无报错
3、设备回调时间与PC当前时间一致
4、设备静置24小时以上回调时间与pc当前时间差<=2s

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 860 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [clock864-acceptance-989.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/clock864-acceptance-989.md)
- [checkpoint989-clock-acceptance/xts-current-status.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint989-clock-acceptance/xts-current-status.md)
- [checkpoint989-clock-acceptance/xts-category-status.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint989-clock-acceptance/xts-category-status.md)
- [checkpoint989-clock-acceptance/clock864-acceptance-989.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint989-clock-acceptance/clock864-acceptance-989.md)
- [checkpoint989-clock-acceptance/board-only-after-longrun.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint989-clock-acceptance/board-only-after-longrun.md)
- [checkpoint989-clock-acceptance/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint989-clock-acceptance/SHA256SUMS)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
