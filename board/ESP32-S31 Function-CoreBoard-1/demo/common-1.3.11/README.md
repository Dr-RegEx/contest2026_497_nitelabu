# Uart文件传输功能测试

## 已通过范围

- 1.3.11：🟢 已通过；805：129字节/65573字节文件双向传输一致

在 NSH 与主机 YMODEM 客户端之间双向发送 129 字节和 65573 字节文件，并比对内容；不能只看传输退出码。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/system/ymodem](../_sources/apps/system/ymodem)：原工程 `apps/system/ymodem`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-xts-ymodem](../../configs/demo-rmt-xts-ymodem/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-xts-ymodem 8
```

产物位于 `out/esp32s31-demo-rmt-xts-ymodem/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、参考以下文档进行测试

**预期结果：**

文件发送和接收功能均正常

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 764 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

### 历史板端命令摘录

下面从通过记录抽取核心命令用于核对参数，不是自动执行脚本；完整时序、复位/断电位置和循环次数仍以原文与日志为准。历史目录只作为示例，新测试使用自己的独立目录。

```text
mkdir /tmp/xts805
```

来源：[xts805-ymodem.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts805-ymodem.log)。

## 历史通过依据

- [checkpoint805-xts.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint805-xts.md)
- [logs/xts805-ymodem.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts805-ymodem.log)
- [xts-current-status.md](../../../../docs/acceptance/xts-current-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
