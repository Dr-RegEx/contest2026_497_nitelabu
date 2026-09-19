# Crypto功能测试

## 已通过范围

- 1.3.17：🟢 已通过；1010：原始8个Crypto程序通过；部分算法软件实现，硬件范围见历史记录

原始 8 个 Crypto 程序分别执行；软件回退与硬件加速范围见日志，不能把 API 通过等同于全部硬件加速。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/testing/drivers/crypto](../_sources/apps/testing/drivers/crypto)：原工程 `apps/testing/drivers/crypto`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-xts-ecc-offline-log](../../configs/demo-rmt-xts-ecc-offline-log/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-xts-ecc-offline-log 8
```

产物位于 `out/esp32s31-demo-rmt-xts-ecc-offline-log/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、确认厂商硬件实现的算法，并在nsh中输入对应算法名称，等待执行结果
举例：若厂商硬件实现了des3cbc算法，即在nsh中输入 des3cbc即可

**预期结果：**

测试程序正常运行结束，并显示结果ok

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 972 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
cmocka_des3cbc
cmocka_aescbc
cmocka_aesctr
cmocka_aesxts
cmocka_hmac
cmocka_hash
cmocka_crc32
cmocka_ecdsa
```

来源：[xts1010-original-crypto.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1010-original-crypto.log)。

## 历史通过依据

- [checkpoint1010-crypto-target/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1010-crypto-target/result.json)
- [checkpoint1010-crypto-target/xts-current-status.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1010-crypto-target/xts-current-status.md)
- [checkpoint1010-crypto-target/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1010-crypto-target/SHA256SUMS)
- [logs/xts1010-original-crypto.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1010-original-crypto.log)
- [checkpoint1010-crypto-target/logs/xts1010-original-crypto.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1010-crypto-target/logs/xts1010-original-crypto.log)
- [checkpoint1010-crypto-target/logs/xts1009-flash1002-crypto.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1010-crypto-target/logs/xts1009-flash1002-crypto.log)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
