# SIMD指令集与PIE上下文保护

## 已通过范围

- 用户自添加第2项：🟢 已验证所列范围；1566：两线程各100轮向量加法/INT8点积，200组点积一致；每线程20次CPU迁移；睡眠切换寄存器保护；1633/1654回归

NSH 执行 s31simd；两线程各 100 轮向量加法/INT8 点积和 PIE 全寄存器保护，各 20 次绑核迁移。200 组点积须与标量对照一致；不宣称 AI 模型或加速倍数。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/examples/s31simd](../_sources/apps/examples/s31simd)：原工程 `apps/examples/s31simd`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-bttool-coex-pie](../../configs/demo-rmt-bttool-coex-pie/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-bttool-coex-pie 8
```

产物位于 `out/esp32s31-demo-rmt-bttool-coex-pie/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

NSH 执行 s31simd；两线程各 100 轮向量加法/INT8 点积和 PIE 全寄存器保护，各 20 次绑核迁移。200 组点积须与标量对照一致；不宣称 AI 模型或加速倍数。

## 历史通过依据

- [checkpoint1566-simd-int8/review.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1566-simd-int8/review.md)
- [checkpoint1566-simd-int8/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1566-simd-int8/SHA256SUMS)
- [simd1566-handoff.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/simd1566-handoff.json)
- [logs/simd1566-target.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/simd1566-target.log)
- [checkpoint1566-simd-int8/simd1566-target.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1566-simd-int8/simd1566-target.log)
- [checkpoint1633-tick-catchup/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1633-tick-catchup/SHA256SUMS)
- [clock1633-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/clock1633-result.json)
- [logs/host1633-warm.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/host1633-warm.log)
- [logs/clock1633-load.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/clock1633-load.log)
- [logs/host1633-clock-load.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/host1633-clock-load.log)
- [logs/clock1633-simd-regression.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/clock1633-simd-regression.log)
- [checkpoint1654-offline-isolation/review.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1654-offline-isolation/review.md)
- [network1654-offline-isolation.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/network1654-offline-isolation.json)
- [logs/network1654-offline-isolation.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/network1654-offline-isolation.log)
- [checkpoint1654-offline-isolation/network1653-reobserve.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1654-offline-isolation/network1653-reobserve.json)
- [checkpoint1654-offline-isolation/network1654-offline-isolation.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1654-offline-isolation/network1654-offline-isolation.log)
- [checkpoint1654-offline-isolation/network1654-offline-isolation.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1654-offline-isolation/network1654-offline-isolation.json)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
