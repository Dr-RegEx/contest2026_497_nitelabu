# SD 卡与 USB2.0 实物核验顺序（1753）

本记录只描述下一轮实物核验，不把静态审计或候选镜像当作通过证据。

## SDMMC/SDIO

Function-CoreBoard-1 官方引出的 slot 0 信号位于 J2：

|信号|GPIO|J2|
|---|---:|---:|
|SD_D0|20|26|
|SD_D1|21|27|
|SD_D2|22|28|
|SD_D3|23|29|
|SD_CLK|24|30|
|SD_CMD|25|31|

接线前必须确认外接模块的 3.3 V 供电、共地、CMD/D0-D3 上拉和模块是否已经包含电平转换。先使用 1-bit 或低速初始化读取 CID/CSD，再扩大到 4-bit；顺序为：

1. Host 初始化和卡识别；
2. 容量、CID/CSD、总线宽度日志；
3. 原始块读写并逐块比对；
4. FAT 挂载、文件创建/读取/删除；
5. 受控掉电后重新识别并复核文件。

在 `sdio_dev_s` 桥接、`/dev/mmcsd0` 注册和串口日志完成前，不刷默认镜像，也不把“能看到 GPIO/HAL”记为协议通过。

## USB2.0

开发板 Type-A 是 S31 OTG High-Speed **Host** 口，官方资料给出最高 500 mA 输出。Host 核验应使用 U 盘、USB 键盘或其他可识别 USB 设备，记录 VBUS、设备速度、VID/PID、配置和至少一次端点传输。

当前工程只有 `ESP32S31_USBDEV + USBADB` Device 候选；USB-UART、Serial-JTAG、TCP ADB 都不能替代 Type-A Host 枚举，也不能替代 xTS 4.2.2 要求的外部主机 `adb shell` 会话。USB Host 初始化和枚举注册接入前，不刷写该候选镜像进行实测。

## 需要用户准备

- 一张可擦写 microSD 卡和带 3.3 V/上拉的 SDIO 模块，或确认模块已集成这些电路；
- 一个低功耗 USB 设备用于 Type-A Host 枚举；
- 确认 UART 仍连接 `/dev/ttyUSB0`，实测时不占用同一串口；
- 接线和插拔均在断电状态进行，避免误接 J2 GPIO 或 VBUS。

