# 文件系统fstest执行1000次

## 已通过范围

- 5.1.15：🟢 已通过；PASS：1181原始1000轮，1184收尾

保留原始 1000 轮，不以少量循环替代；测试使用独立卷和目录。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/testing/fs/fstest](../_sources/apps/testing/fs/fstest)：原工程 `apps/testing/fs/fstest`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[xts-flat-category-fs-name](../../configs/xts-flat-category-fs-name/defconfig)。
- 镜像类型：FLAT（仅 nuttx.bin）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-category-fs-name 8
```

产物位于 `out/esp32s31-xts-flat-category-fs-name/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、在nsh中依次输入如下命令：
mkdir DIR/fstest
fstest -m DIR/fstest -n 1000
rm -r DIR/fstest
注：DIR为需要测试的文件系统挂载的目录，可通过df -h来查看，例如/data（默认）， /sdcard， /sst，rpmsgfs(一般非主核挂载)

**预期结果：**

1、程序正常执行,无crash,没有输出error
注：有的设备运行1000次需要2天时间

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 2111 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
mkdir /data/fstest
fstest -m /data/fstest -n 1000
```

来源：[xts1181-fstest1000.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1181-fstest1000.log)。

## 历史通过依据

- [fs1181-running.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/fs1181-running.json)
- [logs/xts1181-fstest1000.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1181-fstest1000.log)
- [checkpoint1184-fstest1000/xts1181-fstest1000.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1184-fstest1000/xts1181-fstest1000.log)
- [checkpoint1184-fstest1000/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1184-fstest1000/result.json)
- [checkpoint1184-fstest1000/fs1183-verdict-note.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1184-fstest1000/fs1183-verdict-note.md)
- [checkpoint1184-fstest1000/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1184-fstest1000/SHA256SUMS)
- [fs1184-finish.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/fs1184-finish.json)
- [logs/xts1184-fstest-finish.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1184-fstest-finish.log)
- [checkpoint1184-fstest1000/postcheck.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1184-fstest1000/postcheck.log)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
