# USB1763 S31 HCD ABI audit

日期：2026-09-19。此记录仅核对锁定 NuttX `usbhost.h` 与当前 S31 Host 候选的 ABI 表面，不进行刷写、串口操作或外部设备测试，也不把初始化候选当成 USB 协议通过。

## 可复现检查

运行：

```sh
bash backups/2026-09-10-scan-stress/usb1763-hcd-api-audit.sh
```

结果（当前源树）：

```text
USB1763_API_CHECK_BEGIN
wait=MISSING
enumerate=MISSING
ep0configure=MISSING
epalloc=MISSING
epfree=MISSING
alloc=MISSING
free=MISSING
ioalloc=MISSING
iofree=MISSING
ctrlin=MISSING
ctrlout=MISSING
transfer=MISSING
cancel=MISSING
disconnect=MISSING
asynch=MISSING_CONDITIONAL
connect=MISSING_CONDITIONAL
DWC_HCCHAR=MISSING
DWC_HCTSIZ=MISSING
DWC_HCDMA=MISSING
DWC_HAINT=MISSING
DWC_HAINTMSK=MISSING
NUTTX_HOST_REGISTRATION=MISSING
USB1763_REQUIRED_MISSING=14
USB1763_STATUS=HCD_INCOMPLETE
USB1763_API_CHECK_END
```

`HCCHAR/HCTSIZ/HCDMA/HAINT/HAINTMSK` 在锁定 HAL 的寄存器定义中存在，但当前候选源没有访问这些 Host channel/IRQ 状态寄存器；因此审计结果为源实现缺失，而不是芯片能力缺失。可复现脚本只检查当前候选源，避免把头文件中存在的寄存器误报成已实现。

## 结论与下一步

当前 `esp32s31_usbhost_initialize()` 只完成 UTMI/PHY、核心复位、Host mode、FS48MHz 和 VBUS 前置。要接入 NuttX Host，必须实现 `usbhost_connection_s` 的 `wait/enumerate`，`usbhost_driver_s` 的端点、控制/批量/中断传输、DMA 缓冲、取消和断开回调，并接入 DWC Host channel 调度与 IRQ 完成路径；随后还需 root-hub 复位、设备地址分配、配置描述符读取和至少一个 class driver。

因此本轮只增加严格、可复核的接口边界证据，**不增加 USB2.0 或 xTS 4.2.2 通过数**。默认配置保持关闭；只有在 HCD、枚举和真实外部设备端点收发均有证据后，才可改变清单状态。
