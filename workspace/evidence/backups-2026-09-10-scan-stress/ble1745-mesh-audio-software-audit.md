# BLE Mesh / LE Audio 软件审计（1745）

日期：2026-09-18  
范围：只检查不依赖第二块实物的源码、配置和链接证据；不把静态证据写成空口互通通过。

## BLE Mesh 1.1

现有候选 `ble1728-mesh-build.md` 对应镜像完成 1,788 个 Ninja 目标，且
`.config` 已启用 `CONFIG_BT_MESH`、PB-ADV、PB-GATT、Provisionee、Relay、
GATT Proxy；`nuttx.map` 能解析 `bt_mesh_init` 和 `bt_mesh_reset`。这证明
Mesh 子系统已被编译并链接。

该 profile 的 `CONFIG_BT_MESH_SHELL` 未启用，镜像也没有应用调用
`bt_mesh_init()`，所以不能从当前镜像得到板上初始化、广播或 provision 运行证据。
仓库自带 `tests/bluetooth/mesh_shell` 示例确实调用了 `bt_enable()`、
`bt_mesh_init()` 和 PB-ADV/PB-GATT，但默认 openvela CMake 未将它接入该板的
profile。一次隔离接入尝试还暴露出旧版 ZBlue shell 的 `bt_ready` 参数不匹配、
`CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC` 兼容宏缺失等问题；该失败候选未留在工程、
未生成收据、未刷写开发板。

结论：Mesh 保持“候选镜像已构建、待双节点实测”，不增加 xTS 或 Mesh 通过数。

## LE Audio / ISO

`ble1727-ble-audio-mesh-build.md` 与 `build1727-ble-audio-mesh.sha256` 证明
ISO central/peripheral/broadcast、BAP unicast client 和 broadcast source 等
配置完成全量链接；这是实现可编译性证据。当前 profile 没有可用的 LE Audio
对端验证路径，也没有从 ESP32-S31 controller HCI 能力确认 CIS/BIG 建立成功。

结论：LE Audio 保持“ZBlue ISO/BAP 静态构建完成、待对端互通”，不增加 xTS 或
LE Audio 通过数。

## 后续最小验证路径

若继续做单板软件验证，应先单独修正 ZBlue shell 的 API/时钟兼容并生成带
`mesh_shell` 应用的隔离镜像，再刷写后仅记录 `bt_enable`、`bt_mesh_init`、
PB-ADV/PB-GATT bearer 启动日志。该日志只能将 Mesh 状态推进到“板上初始化已验证”；
provision、model 收发仍需要第二节点。LE Audio 则必须提供支持 ISO 的对端，单板
无法替代 CIS/BIG 空口互通。

