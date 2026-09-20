# USB 2.0 角色与工程入口审计（用户自添加第 12 项）

更新时间：2026-09-18

## 结论

Function-CoreBoard-1 的 USB2.0 Type-A 物理口按官方指南是 S31 的 OTG High-Speed Host 口；当前工程已有的是 Device/ADB 候选，尚未形成 Type-A Host 枚举证据，也未完成 xTS 4.2.2 的 ADB shell 会话。

## 2026-09-18 复核

- 通用 NuttX `drivers/usbhost` 源码存在，但本板 S31 架构目录没有 `CONFIG_USBHOST` 分支、Host 控制器源文件或 `usbhost_initialize()`/端口枚举注册；仅打开通用 Host Kconfig 不会形成可运行 Host 适配。
- `openvela-dev/out/esp32s31-xts-flat-usb-adb/.config` 只确认 `CONFIG_ESP32S31_USBDEV=y`、`CONFIG_USBDEV=y`、`CONFIG_USBADB=y`、`CONFIG_SYSTEM_ADBD=y`、`CONFIG_ADBD_USB_SERVER=y`、`CONFIG_ADBD_SHELL_SERVICE=y`，没有 `CONFIG_USBHOST`；这是 Device/ADB 隔离候选。
- `build965-flat-usb-adb.sha256` 已对当前 `nuttx.bin` 复核一致：`8f7e4571aa16ea74a7e6c3a60aaabdccb1d99db6ba6dea14f81292a86ee2bb81`。该 FLAT 候选没有 AppFS 配对镜像，不能与其他镜像混配，也没有因此产生目标板证据。
- 原始 4.2.2 的唯一通过条件仍是外部主机通过 USB 枚举设备并成功执行 `adb shell`；串口 UART、Serial-JTAG、TCP ADB 或本地 `adbd` 进程均不能替代该会话。

## 2026-09-18 Host 候选边界

已加入一个默认关闭的 `CONFIG_ESP32S31_USBHOST` 候选路径：

- `arch/risc-v/src/esp32s31/esp32s31_usbhost.c` 可在打开 `CONFIG_USBHOST` 时编译；初始化会先关闭 Device 全局中断门控，再强制 OTG Host 模式，选择 FS48MHz Host 时钟并请求 Type-A VBUS。
- `arch/risc-v/src/esp32s31/include/esp32s31_usbhost.h` 提供 `esp32s31_usbhost_initialize()`，板级 `esp32s31_bringup.c` 仅在该选项显式启用时调用。
- `arch/risc-v/src/esp32s31/Make.defs` 只在 `CONFIG_ESP32S31_USBHOST=y` 时编译该源文件；普通生产、xTS 和 ADB 配置不受影响。
- 该候选不打开 USB 中断，也没有 `usbhost_driver_s`、`usbhost_connection_s`、根 Hub 等待/枚举或端点传输实现；因此不能宣称 USB2.0 枚举、MSC/HID 或 xTS4.2.2 通过。
- 同一 `xts-flat-usb-adb` 交叉编译参数下，Host 候选源文件和板级 bring-up 条件编译均已通过（目标文件输出到 `/tmp`，未改默认镜像）。

另外修正了 `esp32s31_otg.h` 中 `ESP32S31_OTG_CHAN_OFFSET()` 缺少右括号的问题；该错误此前只有 Host 通道宏展开时才会暴露。

## 已确认

- 官方板级指南将 USB 2.0 Type-A 描述为连接 S31 OTG High-Speed 接口的 Host 口，并提供最高 500 mA 输出。
- 芯片侧寄存器头 `esp32s31_otg.h` 同时包含 Host 和 Device 寄存器区域，说明硬件控制器具备 OTG 寄存器模型。
- 当前 S31 NuttX 适配只在 `esp32s31_usbdev.c` 和 `CONFIG_ESP32S31_USBDEV` 路径接入 Device 控制器。
- 当前架构 Kconfig 的 `ESP32S31_USBDEV` 帮助文本也明确这是“USB device controller (full-speed PIO)”；其测试说明要求 Type-A VBUS 及隔离的 Device 连接，不能据此推导出 Host 驱动。
- `xts-flat-usb-adb` 仅打开 `CONFIG_USBDEV`、`CONFIG_ESP32S31_USBDEV`、`CONFIG_USBADB`，属于 Device/ADB 候选；现有 `checkpoint965-usb-utmi.md` 只有构建证据。
- 当前板级配置和源码没有发现 `CONFIG_USBHOST` 或 `usbhost_initialize()` 的 S31 bring-up 注册。

## 当前缺口

1. 已有默认关闭的 Host 模式寄存器初始化入口，但仍没有 Type-A Host 控制器到 NuttX USB Host 框架的完整适配。
2. 没有 VBUS、端口状态、设备枚举、控制/批量/中断端点传输日志。
3. xTS 4.2.2 的 ADB 语义是主机通过 USB 连接设备并执行 `adb shell`；这与板上 Type-A Host 方向不是同一个物理角色，必须先确定比赛验收采用的 USB 角色和端口。

## 后续路径

优先确认验收是 Type-A Host 协议测试还是需要另一个 USB Device 端口。若测 Type-A Host，先接入 NuttX USB Host/OTG Host 初始化和一个可识别的 USB 设备，再记录枚举及端点传输；若仍要求 ADB Device，则必须明确可用的 Device 物理连接，不能使用 USB-UART 或 Serial/JTAG 代替。

硬件依据：<https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp32-s31-function-coreboard-1/user_guide.html>。

## 2026-09-18 Host 候选边界

已加入一个默认关闭的 `CONFIG_ESP32S31_USBHOST` 候选路径。它可在打开 `CONFIG_USBHOST` 时编译，初始化会先关闭 Device 全局中断门控，再强制 OTG Host 模式、选择 FS48MHz Host 时钟并请求 Type-A VBUS。`Make.defs` 仅在该选项显式启用时编译，普通生产、xTS 和 ADB 配置不受影响。

该候选不打开 USB 中断，也没有 `usbhost_driver_s`、`usbhost_connection_s`、根 Hub 等待/枚举或端点传输实现，因此不能宣称 USB2.0 枚举、MSC/HID 或 xTS4.2.2 通过。同一 `xts-flat-usb-adb` 交叉编译参数下，Host 候选源文件和板级 bring-up 条件编译均已通过，目标文件输出到 `/tmp`，未改默认镜像。

另外修正了 `esp32s31_otg.h` 中 `ESP32S31_OTG_CHAN_OFFSET()` 缺少右括号的问题；该错误此前只有 Host 通道宏展开时才会暴露。
