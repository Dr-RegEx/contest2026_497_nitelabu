# RNG功能测试

## 已通过范围

- 1.3.16：🟢 用户确认通过；1682用户确认通过；786原始26项未定义统计输出及808补充记录保留，不重跑

**用户确认通过**：保留原始 26 项未定义统计输出，不将其改写成统计套件全通过或社区认证。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/testing/drivers/nist-sts](../_sources/apps/testing/drivers/nist-sts)：原工程 `apps/testing/drivers/nist-sts`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-xts-rng](../../configs/demo-rmt-xts-rng/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-xts-rng 8
```

产物位于 `out/esp32s31-demo-rmt-xts-rng/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、按下述方法执行：
cd /tmp
mkdir -p experiments/AlgorithmTesting/ApproximateEntropy
mkdir -p experiments/AlgorithmTesting/CumulativeSums
mkdir -p experiments/AlgorithmTesting/Frequency
mkdir -p experiments/AlgorithmTesting/LongestRun
mkdir -p experiments/AlgorithmTesting/OverlappingTemplate
mkdir -p experiments/AlgorithmTesting/RandomExcursionsVariant
mkdir -p experiments/AlgorithmTesting/Runs
mkdir -p experiments/AlgorithmTesting/Universal
mkdir -p experiments/AlgorithmTesting/BlockFrequency
mkdir -p experiments/AlgorithmTesting/FFT
mkdir -p experiments/AlgorithmTesting/LinearComplexity
mkdir -p experiments/AlgorithmTesting/NonOverlappingTemplate
mkdir -p experiments/AlgorithmTesting/RandomExcursions
mkdir -p experiments/AlgorithmTesting/Rank
mkdir -p experiments/AlgorithmTesting/Serial
nist_sts 400000
2、进入测试程序后依次输入：
0
/dev/urandom/
1
0
10
1
3、查看结果：cat /tmp/experiments/AlgorithmTesting/finalAnalysisReport.txt

**预期结果：**

P-Value是全部测试结果的卡方分布的累计值，其值大于0.0001即可认为该随机数样本具有足够的均匀性与独立性，当值不够随机时在值后以及测试项名称前会有/*标记该项

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 918 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
mkdir -p experiments/AlgorithmTesting/ApproximateEntropy
mkdir -p experiments/AlgorithmTesting/CumulativeSums
mkdir -p experiments/AlgorithmTesting/Frequency
mkdir -p experiments/AlgorithmTesting/LongestRun
mkdir -p experiments/AlgorithmTesting/OverlappingTemplate
mkdir -p experiments/AlgorithmTesting/RandomExcursionsVariant
mkdir -p experiments/AlgorithmTesting/Runs
mkdir -p experiments/AlgorithmTesting/Universal
mkdir -p experiments/AlgorithmTesting/BlockFrequency
mkdir -p experiments/AlgorithmTesting/FFT
mkdir -p experiments/AlgorithmTesting/LinearComplexity
mkdir -p experiments/AlgorithmTesting/NonOverlappingTemplate
mkdir -p experiments/AlgorithmTesting/RandomExcursions
mkdir -p experiments/AlgorithmTesting/Rank
mkdir -p experiments/AlgorithmTesting/Serial
mkdir templates
```

来源：[xts786-nist-rng-clock.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts786-nist-rng-clock.log)、[xts808-nist100.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts808-nist100.log)。

## 历史通过依据

- [logs/xts786-nist-rng-clock.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts786-nist-rng-clock.log)
- [logs/xts808-nist100.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts808-nist100.log)
- [rng1682-user-acceptance.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/rng1682-user-acceptance.md)
- [sprint1682-current-cases.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/sprint1682-current-cases.json)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
