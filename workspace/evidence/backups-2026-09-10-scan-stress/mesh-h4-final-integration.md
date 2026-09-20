# Mesh/SAR 候选合入最新 H4 发送修复

2026-09-19，清单 IV10。没有新增协议功能，没有刷板。

先前 Mesh/SAR 冻结镜像早于 [ISO/H4所有权修复](ble-iso-ownership-result.md)，因此不能拿 LE Audio 新镜像的构建结果推断旧 Mesh 镜像已包含相同修复。本轮对 `xts-flat-ble-mesh-init` 在全新目录重新构建，保留旧镜像。

- 脚本 `build-mesh-h4-final.sh`，完整构建 exit0；源码配置已恢复。
- 保留 Mesh、PB-ADV/PB-GATT、cfg/health/SAR 模型、shell 及显式模型注册，配置/符号检查通过。
- 本profile未定义 CONFIG_BT_ISO。按最终生成config优先include重新严格编译 `esp32s31_ble.c` 和实际framework `hci_h4.c`，均 `-Werror` 通过，日志 `mesh-h4-final-strict.log`。
- 主机发送逻辑回归复用 `ble-iso-ownership-host-test.log` 已通过结果；本轮不重跑相同测试，不新增实物通过数。
- 输出 `openvela-dev/out/esp32s31-xts-flat-ble-mesh-h4-final-20260919/nuttx.bin`，1402704字节，SHA256 `8f26b52f3f327eb1ab98eb376bff5a06c8fc219820124c7b7863ac4813c63583`。
- 完整构建 `logs/build-mesh-h4-final.log`；源码、配置与镜像收据 `mesh-h4-final.sha256`。

仍需真实初始化、配网与模型对端收发。SETTINGS仍未启用，重启后需重新provision，见 [持久化范围](mesh-settings-scope-review.md)。此前初始化-19的板测记录保留，不能用新构建覆盖为实物通过。
