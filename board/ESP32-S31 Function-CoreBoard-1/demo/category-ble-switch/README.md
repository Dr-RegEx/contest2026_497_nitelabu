# 开关BLE蓝牙

## 已通过范围

- xTS原文未编号：🟢 已通过；PASS1085 原始完整开关

NSH 启动 bttool 后在 bttool 提示符依次 enable、state、disable、state、quit；state 应完成 2→0。蓝牙开关通过不等同于其他 BLE 用例通过。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [frameworks/connectivity/bluetooth/tools](../_sources/frameworks/connectivity/bluetooth/tools)：原工程 `frameworks/connectivity/bluetooth/tools`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[xts-flat-bttool](../../configs/xts-flat-bttool/defconfig)。
- 镜像类型：FLAT（仅 nuttx.bin）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-bttool 8
```

产物位于 `out/esp32s31-xts-flat-bttool/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、测试设备执行enable
2、测试设备执行state
3、测试设备执行disable
4、测试设备执行state

**预期结果：**

1、命令执行成功，回调Adapter state changed: 2
2、命令执行成功，回调[bttool] Adapter State: 2
3、命令执行成功，回调Adapter state changed: 0
4、命令执行成功，回调[bttool] Adapter State: 0

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 4968 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [checkpoint1085-ble-switch/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1085-ble-switch/result.json)
- [checkpoint1085-ble-switch/checkpoint1077-bttool-disable.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1085-ble-switch/checkpoint1077-bttool-disable.md)
- [checkpoint1085-ble-switch/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1085-ble-switch/SHA256SUMS)
- [logs/xts1085-ble-switch.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1085-ble-switch.log)
- [checkpoint1085-ble-switch/logs/xts1085-ble-switch.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1085-ble-switch/logs/xts1085-ble-switch.log)
- [checkpoint1085-ble-switch/logs/xts1084-flash1077-ble.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1085-ble-switch/logs/xts1084-flash1077-ble.log)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
