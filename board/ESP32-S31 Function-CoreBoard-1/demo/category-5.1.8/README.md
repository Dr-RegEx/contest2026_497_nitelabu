# 文件系统多线程写文件测试vela_fs_multi_thread_write_test

## 已通过范围

- 5.1.8：🟢 已通过；PASS 原始工作量

使用已备份的专用 LittleFS 测试卷和独立工作目录。参数、循环数及挂载方式以原始步骤和对应通过日志为准；禁止把运行目录设为系统 /etc 或其他已有数据目录。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [tests/testcases/vela_fs_test](../_sources/tests/testcases/vela_fs_test)：原工程 `tests/testcases/vela_fs_test`。

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

### S31 测试卷准备

先执行 `mount`、`df` 核对设备和挂载点。本例当前配置注册 `/dev/xtsflash`（1MiB，Flash 区间 0xc00000–0xcfffff）。LARGE 与普通配置使用同一个节点名但映射到不同分区，不能仅看设备名判断旧卷的容量和内容。不要覆盖已有 `/data` 挂载。

确认对应测试区已备份且为可牺牲数据后，首次建卷可使用 LittleFS 的 `forceformat` 挂载选项；已有卷及掉电/crash 后的恢复必须普通挂载，不能 forceformat。需要精确复核历史结果时，使用历史记录对应的配置和分区。

下面为已有卷的挂载示例；仅在 `/data` 未挂载时使用。`sync` 用于要求持久性的测试，历史吞吐或耗时结果以原始记录的挂载选项为准：

```text
mkdir -p /data
mount -t littlefs -o sync /dev/xtsflash /data
```

为本例程新建独立子目录，把原文 `<DIR>` 替换成该路径；原始工作量按下方命令/步骤保留。测试结束保留结果文件和日志，重测不要混入上次遗留数据。

1、 执行vela_fs_multi_thread_write_test testPath=<DIR> dataLen=xxx
注：
1）dataLen可随意设置，如1000
2）DIR为需要测试的文件系统挂载的目录，可通过df -h来查看，例如/data（默认）， /sdcard， /sst，rpmsgfs(一般非主核挂载)

**预期结果：**

1、输出TEST PASSED

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 1180 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
mkdir /data/s08
vela_fs_multi_thread_write_test -d /data/s08 -l 1000
```

来源：[xts1537-fs-5.1.8.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1537-fs-5.1.8.log)。

## 历史通过依据

- [checkpoint1537-arp-window-negative/network1534-arp-window-review.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1537-arp-window-negative/network1534-arp-window-review.md)
- [checkpoint1537-arp-window-negative/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1537-arp-window-negative/SHA256SUMS)
- [network1537-handoff.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/network1537-handoff.json)
- [logs/xts1537-fs-5.1.8.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1537-fs-5.1.8.log)
- [logs/network1537-cold-tcp.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/network1537-cold-tcp.log)
- [network1537-tcp-statistics-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/network1537-tcp-statistics-result.json)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
