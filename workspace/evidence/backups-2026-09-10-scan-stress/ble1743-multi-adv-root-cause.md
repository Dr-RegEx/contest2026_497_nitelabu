# BLE 多实例广播 status=2 根因与候选构建

更新时间：2026-09-18。此记录包含源码分析、候选构建及 1744 实板复测；手机对端观察未完成，不把候选或板级回调单独计入完整 xTS 通过。

## 现象

`ble1741-same-public/uart.log`、`ble1741-same-random-id/uart.log`、`ble1741-public-random-id/uart.log` 和 `ble1742-ext-multi/uart.log` 均表现为第一路回调 `status:0`，第二路回调 `status:2`。扩展广告诊断中，第二路完成 HCI `2036`、`2035`、`2037`（创建/参数/数据），但没有完成 `2039` enable；这说明失败发生在主机启动前，不是手机扫描未发现。

## 根因定位

1. SAL 在控制器支持扩展广播时走 `frameworks/connectivity/bluetooth/service/stacks/zephyr/sal_le_advertise_interface.c:387-407`，每一路调用 `bt_le_ext_adv_create()`。`apps/external/zblue/zblue/include/zephyr/bluetooth/bluetooth.h:1405-1413` 将其内联到 `bt_le_ext_adv_create_mc()`，因此两路确实使用独立广告对象；不是 `hdev->adv` 单例复用导致的失败。
2. `apps/external/zblue/zblue/subsys/bluetooth/host/adv.c:1673-1688` 的 `bt_le_ext_adv_start()` 对可连接广播先调用 `le_adv_start_add_conn()`。该函数在 `adv.c:932-949` 为每个未定向可连接广告申请一个 `bt_conn`，连接池耗尽返回 `-ENOMEM`。
3. 当前实板镜像 `openvela-dev/out/esp32s31-ble-static1725/.config` 为 `CONFIG_BT_MAX_CONN=1`，而 `CONFIG_BT_EXT_ADV_MAX_ADV_SET=2`。SAL 将启动错误折叠为 `BT_STATUS_FAIL`（`sal_le_advertise_interface.c:403-407`），bttool 因而显示数值 `status:2`。

当控制器不支持扩展广播时，SAL 会在 `sal_le_advertise_interface.c:409-416` 回落到 `bt_le_adv_start()`；该兼容 API 在 `adv.c:1379-1404` 确实通过 `adv_create_legacy(hdev)` 维护单一 legacy 广播对象。因此 legacy 回落路径仍不能宣称支持多路，需单独修复或限制测试条件。当前 ESP32-S31 镜像已报告扩展广播能力，已观测到的是扩展路径的连接池耗尽。

## 候选修复与构建

在 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-rmt-bttool-kernel/defconfig` 增加 `CONFIG_BT_MAX_CONN=2`，保持两个扩展广告集合各有一个连接对象。候选构建脚本为 `build1743-ble-multi-conn.sh`，输出目录为 `openvela-dev/out/esp32s31-ble-multi1743`。

构建成功并确认：

```text
CONFIG_BT_EXT_ADV_MAX_ADV_SET=2
CONFIG_BT_MAX_CONN=2
nuttx.bin  767736f03bbb3b09a72ab41e2fd3cf18ebafe3fcc27cfa594b695798cf33b2f0
appfs.img  9b1d9944ab6725f4851500ad45d769dd1ab9869e3c6b071b021bfbf6ddb134bb
```

收据：`build1743-ble-multi-conn.sha256`。候选镜像随后完成了开发板双实例复测，结果见下方 1744 记录；手机对端观察仍未完成，因此 5023、5388、5417 不计完整 xTS 通过。

## 1744 实板复测

候选镜像已刷写并完成三组板端复测：相同 public（yuy1/yuy2）、相同 random_id（yuy3/yuy4）、public+random_id（yuy5/yuy6）均获得两路 `adv_id 1/2 status:0`，两路停止回调也成功。证据见 `ble1744-multi-adv.json` 及三个 `ble1744-*/uart.log`。原始 xTS 仍要求手机同时观察两个名称/地址，因此清单改记为板级通过、待手机观察，不计正式 xTS 通过。
