# BLE Audio/Mesh current-source rebuild (1783)

日期：2026-09-19

本轮只复核 BLE Audio、ISO/BAP 和 Mesh 隔离 profile 是否仍能从当前源码完整构建；不刷机、不打开串口、不连接 BLE 对端，不把静态构建结果记为 xTS 或空口互操作通过。

## 构建结果

使用锁定的 S31 工具链和 `build1727-ble-audio-mesh.sh`，从当前源码重新生成 `xts-flat-ble-audio-mesh`：

```text
1796/1796 ninja targets completed
image size: 1304364 bytes
sha256: b69823b3ae4105939807a73a9bfd328d8a18fb3b46421604f4d60f58ed588896
receipt: build1781-ble-audio-mesh.sha256
```

当前 `.config` 保留 `CONFIG_BT_AUDIO`、ISO peripheral/central/broadcaster/sync receiver、BAP unicast/broadcast 相关选项，以及 Mesh PB-ADV/PB-GATT/provisionee/relay/GATT proxy。`nuttx.map` 包含 `bt_iso_chan_connect`、`bt_iso_big_create` 和 `bt_mesh_init`。

## 边界

构建证明当前 BLE Audio/Mesh 源码与 S31 隔离 profile 的配置和链接仍然兼容；它没有证明控制器支持对应 ISO 空口能力，也没有证明 CIS/BIG、手机/耳机互通、Mesh provision/model 收发或 xTS 用例通过。默认生产 profile 未启用该隔离配置，后续仍需真实对端和板上日志。
