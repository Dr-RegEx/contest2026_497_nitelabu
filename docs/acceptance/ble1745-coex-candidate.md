# 1745 BLE/Wi-Fi 共存候选构建

日期：2026-09-18

## 目的

为 xTS 4.1.105（BLE 扫描与 Wi-Fi 共存）和 4.1.104（BLE 广播与 Wi-Fi 共存）准备一份配置一致的板级镜像。当前板上 1743 multi-adv 镜像未启用 `ESPRESSIF_WIFI`，不能直接执行原文配网流程；本候选不改变当前板上镜像。

## 构建结果

候选配置：`demo-rmt-bttool-coex-multi`，继承已审计的 1547 Wi-Fi/BLE 共存配置，并将 `CONFIG_BT_MAX_CONN` 提升为 2，以保留两路扩展广播所需的连接池。

配置核验：

```text
CONFIG_ESPRESSIF_WIFI=y
CONFIG_ESP32S31_BLE=y
CONFIG_BT_EXT_ADV_MAX_ADV_SET=2
CONFIG_BT_MAX_CONN=2
CONFIG_BLUETOOTH_GATT_SERVER=y
CONFIG_BLUETOOTH_TOOLS=y
CONFIG_SMP=y
CONFIG_ARCH_ADDRENV=y
```

产物及配对收据：

|文件|大小|SHA-256|
|---|---:|---|
|`openvela-dev/out/esp32s31-ble-wifi-coex-multi1745/nuttx.bin`|1,597,632|`44e6726fa49af692bfdb75956df69467d74375d9d05dde7fcca8a2951d9499dc`|
|`openvela-dev/out/esp32s31-ble-wifi-coex-multi1745/appfs.img`|3,059,712|`885b02743a7a10d1082ea1bed2d98095568482786176a13ba675c767b88f67f7`|

配对收据：`build1745-ble-wifi-coex-multi.sha256`。构建脚本：`build1745-ble-wifi-coex-multi.sh`。

## 验收边界

这是构建和配置证据，不是 xTS 通过证据；1745 尚未刷写开发板，也没有执行路由器配网、`wapi disconnect`、广播/扫描对端或原文 100 次循环。已有 1551/1564 的 GATT+Wi-Fi 并发记录仍只作为开发验证，不能替代 4.1.104/105 原始流程。下一步需要在用户确认可切换镜像后上板，使用指定 2.4 GHz WPA2 网络，分别按原文执行 4.1.105 和 4.1.104 的 100 次循环并保存完整 UART 日志。
