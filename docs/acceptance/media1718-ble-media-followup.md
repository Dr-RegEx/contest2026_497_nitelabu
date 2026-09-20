# BLE / JPEG / RGB 软件侧复核（1718）

日期：2026-09-18。此轮没有开发板和外围设备，仅执行可复现的主机编译、源码边界检查，不能把静态结果当作板级通过。

## JPEG 与 RGB

按 1711 记录的 vendored libjpeg-turbo 构建流程重新编译全部 JPEG 源文件，并运行 `media1711-jpeg-rgb-host-smoke.c`：

```text
PASS jpeg_encode=690 jpeg_decode=2x2 rgb888_rgb565=PASS
```

这再次证明软件侧 RGB888→JPEG、JPEG→RGB 扫描行和 RGB888↔RGB565 5/6/5 量化规则可构建运行。日志和可复现命令见 [`media1711-jpeg-rgb-host-smoke.md`](media1711-jpeg-rgb-host-smoke.md)。S31 `upper_hal_jpeg`/`esp_hal_jpeg` 仍未接入 NuttX 用户态 API，Camera 仍没有 Function-CoreBoard-1 的 DVP 引脚/传感器实物，因此清单中的四项继续保持“软件自测、待板级验证”或“待适配”。

## BLE 版本与专项能力

检查输出见 [`ble1718-capability-check.log`](ble1718-capability-check.log)：

- S31 SDK 头只声明 `CONFIG_SOC_BLE_50_SUPPORTED`，同时声明 ISO、Audio、Mesh 的 SoC 能力；没有 `SOC_BLE_54_SUPPORTED`。
- 预编译 `libble_app.a` 可见路径为 `ble52_controller_sdk`。NimBLE host 的默认 `MYNEWT_VAL_BLE_VERSION (54)` 不能覆盖 controller 版本证据。
- openvela NimBLE Kconfig 目前最高仅提供 5.3 选项。
- IDF 参考树确实含 `esp_ble_iso`、`esp_ble_audio` 和 Mesh 源目录，但当前保存 profile 未启用 BLE Audio；没有 Mesh 运行时/双节点证据。

因此本轮没有虚构 BLE 5.4、LE Audio 或 Mesh 通过结果。要闭合这些项目，必须先取得 controller HCI version/features，并在有对端时完成 ISO 音频或至少双节点 Mesh 互通；host 默认值和目录存在只能作为待接入线索。

