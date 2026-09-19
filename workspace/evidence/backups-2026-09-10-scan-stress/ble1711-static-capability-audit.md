# BLE 静态能力核验（1711）

日期：2026-09-18  
目标板：ESP32-S31 Function-CoreBoard-1  
范围：比赛必须适配清单中 BLE xTS 18 项、BLE 5.4、LE Audio、BLE Mesh 1.1。

## 结论

本轮只进行了源码、配置和历史证据核验，开发板当前不在手边，因此没有新增空口通过记录。已有的 `1085`（开关 BLE）、`1544/1551/1564`（GATT/并发）等证据继续有效，但不能替代 18 项 xTS 原始用例的手机/对端观察步骤。

BLE 5.4 目前不能宣称已完成。工程同时出现两层不同含义的版本信息：

* `openvela-dev/nuttx/arch/risc-v/src/esp32s31/include/sdkconfig.h` 仅声明 `CONFIG_SOC_BLE_50_SUPPORTED`，并声明 ISO、BLE Audio、BLE Mesh、Periodic Advertising 等能力；没有 `SOC_BLE_54_SUPPORTED` 或 BLE 5.4 控制器版本宏。
* S31 预编译控制器库 `s31-reference/deps/esp-hal-3rdparty/components/bt/controller/lib_esp32s31/esp32s31-bt-lib/libble_app.a` 的可见字符串包含 `ble52_controller_sdk`。这至少说明当前归档来自 BLE 5.2 controller SDK，不能按 5.4 宣称。
* 参考 IDF 的 NimBLE host 配置 `s31-reference/deps/esp-hal-3rdparty/components/bt/host/nimble/port/include/esp_nimble_cfg.h` 默认 `MYNEWT_VAL_BLE_VERSION (54)`，这是 host 编译默认值，不能证明 S31 的预编译 controller radio 已实现 BLE 5.4。其 Kconfig 也只以 `BT_NIMBLE_50_FEATURE_SUPPORT` 作为硬件能力门槛。
* openvela 应用侧 `apps/wireless/bluetooth/nimble/Kconfig` 当前只提供 5.0、5.1、5.2、5.3 选择，没有 5.4 选项。
* 已保存的 BLE profile（例如 `openvela-dev/out/esp32s31-ble-kernel1575/.config`）启用了 peripheral/central/broadcaster/observer、extended advertising、连接、PHY/data length 等基础能力，但 ISO 四种角色和 `CONFIG_BLUETOOTH_BLE_AUDIO` 均未启用。

因此“host 头文件默认 54”只能作为待核对线索；正式版本判定必须取得 controller feature/version HCI 返回值并完成对端互操作记录。

## 与清单的对应关系

|范围|静态核验结果|当前判定|
|---|---|---|
|BLE xTS 18 项|命令/源码和历史记录已定位；广告、扫描 PHY/mode、发现时长/成功率、配对、共存等仍需要手机或 BLE 对端|待人工实物核验|
|BLE 5.4|host 默认值为 54，但 SoC/controller 公开配置只到 BLE 5.0 能力门槛；无 5.4 HCI 证据|待 controller HCI/互操作核验|
|LE Audio|IDF 依赖树有 `components/bt/esp_ble_audio`、ISO host 源码；当前保存镜像 ISO 和 `CONFIG_BLUETOOTH_BLE_AUDIO` 均关闭|待接入和实物互通|
|BLE Mesh 1.1|IDF 依赖树含 `esp_ble_mesh/v1.1`；当前镜像无 Mesh 运行时证据|待接入和至少双节点组网|

## 下一次可执行的人工步骤

1. 使用可启用 BLE 的镜像运行 `bttool`，读取 controller version/LE feature/PHY/extended advertising/periodic advertising/ISO 相关 HCI 返回值并保存原始日志。
2. 按清单 BLE 18 项逐项使用 nRF Connect 或同类对端记录，保留广播间隔四档、两路地址、扫描 PHY/mode、发现/配对成功率和 Wi-Fi 共存证据。
3. 仅在 controller HCI 与互操作均满足后，将 BLE 5.4 状态从“待核验”改为通过；LE Audio/Mesh 仍分别保留独立验证状态。
