# USB Host 寄存器候选验证（1755，用户自添加第 12 项）

更新时间：2026-09-18（离线复核修订）

## 验证结果

`CONFIG_ESP32S31_USBHOST` 默认值为 `n`，并且依赖 `CONFIG_USBHOST`、禁止与现有 `ESP32S31_USBDEV` 同时启用。普通 production、xTS 和 ADB 配置不会编译或调用该候选。

在现有 `xts-flat-usb-adb` 的 S31 交叉编译参数上，仅以临时配置加入 `CONFIG_ESP32S31_USBHOST=1` 和 `CONFIG_USBHOST=1`，完成了：

1. `esp32s31_usbhost.c` 目标文件编译；
2. 板级 `esp32s31_bringup.c` 在 Host 条件分支下的目标文件编译。

本轮又完成了 Host 候选的离线边界修正和独立编译：

1. 复用 S31 官方 `usb_utmi_ll` 完成 UTMI/APB/SYS/PHY 时钟开启、PHY/控制器复位、精确 VBUS 检测、Host 15k D+/D- 下拉和唤醒状态设置；
2. 复用官方 `usb_dwc_ll` 配置 16-bit UTMI 与 5 个 PHY timeout 校准；
3. 增加 `usb_dwc_dev_t` 对 `GRSTCTL`、`HCFG`、`HPRT`、Host channel 0 的 `_Static_assert`；
4. 修正 S31 `GSNPSID` 实际位于 `0x40`（`0x3c` 为保留区）的寄存器别名，并校验 Synopsys ID `0x4f54xxxx`；
5. 增加 AHB idle、S31 reset-done 和 Host mode 切换的超时保护。

`/tmp/s31-usbhost-hcd.o`（2476 字节）由现有 S31 RISC-V 交叉编译参数生成，退出码为 0；`git diff --check` 对相关文件无空白错误。

编译目标均写入 `/tmp`，未刷写、未改变任何默认镜像，也没有产生板上 USB 运行证据。

## 代码边界

候选完成 UTMI/PHY/核心复位、寄存器级 Host 角色准备：关闭 Device 全局中断门控、强制 OTG Host 模式、等待 Host mode 状态、选择 FS48MHz Host 时钟和请求 Type-A VBUS。当前没有：

- Host IRQ 路由和 ISR；
- Host channel 调度、控制/批量/中断端点传输；
- `usbhost_driver_s` 和 `usbhost_connection_s` 实现；
- root-hub 等待、枚举、设备地址分配或 MSC/HID 类绑定。

因此本记录属于“可编译适配边界”，不计 USB2.0 协议通过，也不计 xTS 4.2.2 ADB 通过。下一步必须在隔离 Host 镜像补齐 HCD，再接 USB 设备记录 VBUS、端口状态、枚举和端点传输。
