# 文件系统碎片读写测试

## 已通过范围

- 5.1.16：🟢 已通过；PASS1164/1169 按原文观察标准

专用 3MiB 测试卷，碎片用例保持原观察标准，多线程操作保持原始 1 小时工作量。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [tests/testcases/vela_fs_test](../_sources/tests/testcases/vela_fs_test)：原工程 `tests/testcases/vela_fs_test`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[xts-flat-category-fs-large](../../configs/xts-flat-category-fs-large/defconfig)。
- 镜像类型：FLAT（仅 nuttx.bin）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-category-fs-large 8
```

产物位于 `out/esp32s31-xts-flat-category-fs-large/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

### S31 测试卷准备

先执行 `mount`、`df` 核对设备和挂载点。本例当前配置注册 `/dev/xtsflash`（3MiB，Flash 区间 0xd00000–0xffffff）。LARGE 与普通配置使用同一个节点名但映射到不同分区，不能仅看设备名判断旧卷的容量和内容。不要覆盖已有 `/data` 挂载。

确认对应测试区已备份且为可牺牲数据后，首次建卷可使用 LittleFS 的 `forceformat` 挂载选项；已有卷及掉电/crash 后的恢复必须普通挂载，不能 forceformat。需要精确复核历史结果时，使用历史记录对应的配置和分区。

下面为已有卷的挂载示例；仅在 `/data` 未挂载时使用。`sync` 用于要求持久性的测试，历史吞吐或耗时结果以原始记录的挂载选项为准：

```text
mkdir -p /data
mount -t littlefs -o sync /dev/xtsflash /data
```

为本例程新建独立子目录，把原文 `<DIR>` 替换成该路径；原始工作量按下方命令/步骤保留。测试结束保留结果文件和日志，重测不要混入上次遗留数据。

1、在nsh中依次输入如下命令：
mkdir /DIR/fstest
vela_fs_file_fragmentation_test -d /DIR/fstest -n 1000 -s 20，其中参数说明如下：
-d: set test dir path. -- 测试运行的目录
-n: The number of small files created in the test. -- 测试中产生的小文件数量（每个小文件的大小随机产生，最大不超过128字节）
-s: Large file size created in test. (unit:M)\n") -- 测试中产生的大文件大小上限，单位M
注：
1）测试中根据芯片资源的实际情况，合理填写 -n 和 -s参数
2）DIR为需要测试的文件系统挂载的目录，可通过df -h来查看，例如/data（默认）， /sdcard， /sst，rpmsgfs(一般非主核挂载)

**预期结果：**

1、运行一段时间后无异常

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 2070 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
mkdir /data/frag1164
vela_fs_file_fragmentation_test -d /data/frag1164 -n 100 -s 1 &
```

来源：[xts1164-fragment-observe.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1164-fragment-observe.log)。

## 历史通过依据

- [checkpoint1164-fragment-observation/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1164-fragment-observation/result.json)
- [checkpoint1164-fragment-observation/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1164-fragment-observation/SHA256SUMS)
- [fs1164-fragment-running.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/fs1164-fragment-running.json)
- [logs/xts1164-fragment-observe.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1164-fragment-observe.log)
- [checkpoint1164-fragment-observation/review1169.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1164-fragment-observation/review1169.json)
- [checkpoint1164-fragment-observation/xts1165-fragment-stop.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1164-fragment-observation/xts1165-fragment-stop.log)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
