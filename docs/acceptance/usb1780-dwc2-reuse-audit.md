# USB1780 S31 DWC2 Host driver reuse audit

日期：2026-09-19。此记录针对比赛清单用户自添加第 12 项 USB2.0，只做离线源代码和寄存器布局审计，不刷写、不占用串口、不接外部 USB 设备，也不把 Host 初始化候选当作协议通过。

## 审计命令与结果

运行：

```sh
bash backups/2026-09-10-scan-stress/usb1780-dwc2-reuse-audit.sh
```

当前锁定源树输出：

```text
USB1780_DWC2_REUSE_CHECK_BEGIN
STM32_DWC2_HOST_SOURCE=present
S31_HOST_CANDIDATE_SOURCE=present
STM32_NUTTX_CALLBACKS=14/14
S31_HOST_REG_SYMBOLS=present
S31_HOST_CHANNEL_COUNT=16
STM32_DWC2_ENGINE=present
S31_HOST_HCD_IMPLEMENTATION=init-only
REUSE_CONCLUSION=PORT_STM32_DWC2_HCD_WITH_S31_PLATFORM_ADAPTER
S31_PLATFORM_GAPS=IRQ_VBUS_CLOCK_PHY_FIFO_CONFIG_DMA_CACHE_BOARD_BRINGUP
USB1780_STATUS=HCD_NOT_IMPLEMENTED
USB1780_DWC2_REUSE_CHECK_END
```

## 复用结论

NuttX 已包含完整的 STM32F7 DWC2 OTG Host 实现：`arch/arm/src/stm32f7/stm32_otghost.c` 提供 `usbhost_connection_s` 的等待/枚举路径、全部 `usbhost_driver_s` 端点和控制/同步/异步传输回调、取消/断开处理，以及 root-hub 端口状态、Host channel 调度、FIFO 收发和全局/端点中断处理。该实现与 S31 当前候选都使用 Synopsys DWC2 Host channel 模型，复用方向是可信的。

S31 的 `esp32s31_otg.h` 已有对应的 HCFG/HFNUM/HAINT/HAINTMSK/HPRT、HCCHAR/HCINT/HCINTMSK/HCTSIZ/HCDMA/HCDMAB 和 DFIFO 地址；锁定 Espressif `usb_dwc_struct.h` 还明确有 16 个 Host channel。前一份 1776 护栏已经验证这些结构偏移，说明寄存器布局不是当前主要阻断。

## 不能直接复制的部分

STM32 源文件不能直接纳入 S31：其编译条件、`stm32_getreg/putreg`、IRQ 号和 attach、GPIO/端口 VBUS 控制、PHY 时钟/复位、电源时序、FIFO 深度配置、DMA/缓存一致性和板级 bring-up 都依赖 STM32 平台。S31 当前 `esp32s31_usbhost.c` 只有 UTMI/PHY、核心复位、Host 模式、FS48MHz 与 HPRT VBUS 前置，仍未实现 NuttX Host 回调、通道状态机或中断完成路径。

因此安全路线是先将 STM32 DWC2 Host 引擎拆出可移植核心，再为 S31 提供明确的平台适配层并在 `xts-flat-usb-adb` 之外保持独立配置；不能用空回调、静态“枚举成功”或把 S31 Device/ADB 路径冒充 Host 来闭合测试。该工作量已超过当前无人工/无外设条件下可验证的小改动范围，本轮不修改默认生产配置，也不增加 USB2.0 或 xTS 通过数。
