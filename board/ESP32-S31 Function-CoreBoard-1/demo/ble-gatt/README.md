# BLE GATT实机读写

## 已通过范围

- 用户自添加第1项：🟢 已验证所列范围；1544/1551/1564：FF05/FF02读取，13字节写入及清理

需 BLE 对端；已验证 FF05/FF02 读取、13 字节写入及服务清理。此为开发自测范围，不新增 xTS 编号。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [frameworks/connectivity/bluetooth/tools](../_sources/frameworks/connectivity/bluetooth/tools)：原工程 `frameworks/connectivity/bluetooth/tools`。

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

1. 确认 `/dev/ttyHCI0` 存在，蓝牙数据库目录 `/data/misc/bt` 可写。历史测试在 `/data` 不存在且无持久挂载时创建 tmpfs；不要遮盖已有数据卷。
2. 在 NSH 输入 `bttool`，其后在 `bttool>` 提示符执行：

```text
enable
state
gatts register 3
gatts start 3
adv start -i 160 -n vela-adv-test -m legacy
```

等待 state=2、注册成功与广播 `status:0` 回调。使用电脑或其他 BLE 客户端扫描 `vela-adv-test`，连接并发现服务。读取 16-bit UUID `FF05` 和 `FF02`，两者应返回 `Hello VELA!`；以 Write Without Response 向 `FF02` 写入 ASCII `S31-GATT-1510`（不附加 NUL），共 13 字节：

```text
53 33 31 2d 47 41 54 54 2d 31 35 31 30
```

以板端写回调收到的长度和逐字节内容为准，不以客户端“已提交”作为接收通过证据。测试完成后断开客户端；若广播未自动停止，使用本次 `on_advertising_start_cb` 给出的实际 handle 执行 `adv stop -h <handle>`，再清理：

```text
gatts stop 3
gatts unregister 3
disable
state
quit
```

确认 state=0 且回到 NSH。历史日志另外包含 Wi-Fi 并发 ping，它是所列开发回归的证据，不扩大本例程的 GATT 读写结论。

需 BLE 对端；已验证 FF05/FF02 读取、13 字节写入及服务清理。此为开发自测范围，不新增 xTS 编号。

## 历史通过依据

- [checkpoint1544-ble-smp-mmu-gatt/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1544-ble-smp-mmu-gatt/SHA256SUMS)
- [ble1544-kernel-gatt-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/ble1544-kernel-gatt-result.json)
- [logs/host1544-ble-observation.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/host1544-ble-observation.log)
- [logs/ble1544-kernel-gatt-observe.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/ble1544-kernel-gatt-observe.log)
- [checkpoint1544-ble-smp-mmu-gatt/review.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1544-ble-smp-mmu-gatt/review.json)
- [checkpoint1544-ble-smp-mmu-gatt/host1544-ble-observation.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1544-ble-smp-mmu-gatt/host1544-ble-observation.log)
- [checkpoint1551-coex-gatt-observed/review.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1551-coex-gatt-observed/review.md)
- [checkpoint1551-coex-gatt-observed/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1551-coex-gatt-observed/SHA256SUMS)
- [ble1551-handoff.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/ble1551-handoff.json)
- [ble1551-coexistence-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/ble1551-coexistence-result.json)
- [logs/host1551-ble-observation.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/host1551-ble-observation.log)
- [logs/ble1551-coexistence-observe.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/ble1551-coexistence-observe.log)
- [checkpoint1564-affinity-ble-wifi/review.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1564-affinity-ble-wifi/review.md)
- [checkpoint1564-affinity-ble-wifi/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1564-affinity-ble-wifi/SHA256SUMS)
- [simd1564-handoff.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/simd1564-handoff.json)
- [ble1564-coexistence-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/ble1564-coexistence-result.json)
- [logs/host1564-ble-observation.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/host1564-ble-observation.log)
- [logs/ble1564-affinity-coexistence-observe.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/ble1564-affinity-coexistence-observe.log)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
