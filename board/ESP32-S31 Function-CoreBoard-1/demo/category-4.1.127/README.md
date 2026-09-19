# Audio--nxlooper回环功能测试

## 已通过范围

- 4.1.127：🟢 已通过；PASS1198 原始命令无报错

**通过范围仅为原始命令执行无报错**。日志明确 AUDIBLE_LOOPBACK=PENDING；未完成外接扬声器听验，不等同于声音质量通过。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/system/nxlooper](../_sources/apps/system/nxlooper)：原工程 `apps/system/nxlooper`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[xts-flat-audio-duplex](../../configs/xts-flat-audio-duplex/defconfig)。
- 镜像类型：FLAT（仅 nuttx.bin）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-audio-duplex 8
```

产物位于 `out/esp32s31-xts-flat-audio-duplex/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

S31 录音节点为 `pcm0c`，已将原文通用示例 `pcm13c` 映射为实板节点；原文副本保持原样。

1、在nsh中执行如下命令：
nxlooper
device pcm0p
device pcm0c
loopback 2 16 48000
stop
q

**预期结果：**

1、进入nxlooper后执行命令无报错

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 4548 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [checkpoint1198-nxlooper-commands/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1198-nxlooper-commands/result.json)
- [checkpoint1198-nxlooper-commands/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1198-nxlooper-commands/SHA256SUMS)
- [logs/xts1198-duplex-flash.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1198-duplex-flash.log)
- [logs/xts1198-nxlooper-dma.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1198-nxlooper-dma.log)
- [checkpoint1198-nxlooper-commands/xts1198-duplex-flash.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1198-nxlooper-commands/xts1198-duplex-flash.log)
- [checkpoint1198-nxlooper-commands/xts1198-nxlooper-dma.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1198-nxlooper-commands/xts1198-nxlooper-dma.log)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
