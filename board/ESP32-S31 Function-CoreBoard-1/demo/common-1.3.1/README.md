# 烧写测试

## 已通过范围

- 1.3.1：🟢 已通过；668/669：受保护刷写、哈希及NSH启动

这是烧写/启动操作例程。按根 README 备份 Flash，再写入同一次构建的内核和 AppFS，记录哈希与启动日志。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [nuttx/tools/espressif/esp32s31_simple_boot_digest.py](../_sources/nuttx/tools/espressif/esp32s31_simple_boot_digest.py)：原工程 `nuttx/tools/espressif/esp32s31_simple_boot_digest.py`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-xts-core](../../configs/demo-rmt-xts-core/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-xts-core 8
```

产物位于 `out/esp32s31-demo-rmt-xts-core/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、参照指导文档将版本烧写到flash
2、重启设备，进入设备nsh终端
注意：烧写指导文档和烧录工具由厂商提供，小米根据对应方法进行验收

**预期结果：**

2、能正常烧录，烧录成功后，重启设备，能正常进去nsh终端，系统正常启动

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 506 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [logs/demo669-mqueue-restored.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/demo669-mqueue-restored.log)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
