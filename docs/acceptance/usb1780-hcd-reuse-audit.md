# USB Host HCD 复用边界审计（1780）

日期：2026-09-19

本轮只检查能否复用仓库已有 NuttX USB Host 驱动完成用户自添加第 12 项 USB2.0，未刷写、未打开串口、未接外部 USB 设备。

## 复用检查

现有 `drivers/usbhost` 控制器实现分别面向 MAX3421E、OHCI、Renesas 专用控制器或 PCI xHCI。它们的 Host channel、FIFO/DMA 描述符、IRQ 状态位和 root-hub 电气控制都不是 S31 Synopsys DWC2/UTMI 寄存器布局。S31 当前候选只完成 UTMI/PHY、核心复位、强制 Host、FS48MHz 和 VBUS 前置。

## 不能直接复用的接口

`usbhost_connection_s` 的等待、root-hub 枚举和设备枚举回调，以及 `usbhost_driver_s` 的 EP0 配置、端点分配、控制/批量/中断传输、异步取消和断开回调，都必须由 S31 的 DWC Host channel 调度器驱动。已有驱动的回调骨架不能代替 S31 的 `HCCHAR/HCTSIZ/HCDMA/HAINT` 读写、IRQ 清除、设备地址分配和 FIFO/DMA 完成状态机。

## 结论

1776 的寄存器布局护栏仍是有效的离线增量，但没有可安全移植的现成 HCD 实现。继续拼接其他控制器代码会制造错误的寄存器或 DMA 语义；本轮不添加空回调、不宣称 USB2.0 或 xTS 4.2.2 通过。后续必须实现 S31 专用 HCD，再用 Type-A 外部设备完成真实枚举和端点传输。
