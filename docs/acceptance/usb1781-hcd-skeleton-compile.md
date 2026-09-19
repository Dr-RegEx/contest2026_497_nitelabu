# USB1781 S31 HCD callback/state skeleton compile audit

日期：2026-09-19。本记录是 USB2.0 用户自添加第 12 项的离线适配增量，不代表 Host 枚举、端点传输或 xTS 4.2.2 通过。

## 本轮实现

新增默认关闭的 `CONFIG_ESP32S31_USBHOST_HCD_SKELETON`，仅在同时启用
`CONFIG_ESP32S31_USBHOST` 和 `CONFIG_USBHOST` 时编译。`esp32s31_usbhost_hcd_skeleton.c`
包含：

- 以 `struct usbhost_driver_s` 为首成员的 S31 HCD 状态结构、独立 `usbhost_connection_s` 连接接口、root-hub 容器和 8 个 Host channel 元数据；
- `ep0configure`、端点分配/释放、控制/普通/异步传输、取消、连接和断开等 NuttX HCD 回调的完整 ABI 骨架；
- `usbhost_connection_s.wait/enumerate` 连接监视/枚举回调骨架，明确返回 `-ENOSYS` 并清空输出端口，避免伪造热插拔事件；
- DWC2 `HCCHAR` / `HCTSIZ` 的端点地址、方向、类型、设备地址、最大包长、包数和 PID 编码，以及 RESET→READY 通道状态转换；
- 所有尚未具备 IRQ、锁和 DMA 完成语义的传输回调明确返回 `-ENOSYS`，不触碰 MMIO、不报告枚举成功。

这使后续 S31 专用 HCD 可以在固定 ABI 和寄存器编码上继续实现，同时不会把空回调接入板级启动或默认镜像。

## 严格交叉编译

以 `openvela-dev/out/esp32s31-xts-flat-usb-adb/compile_commands.json` 中锁定的
S31 RISC-V GCC 命令为基础，将源文件临时加入：

```text
-DCONFIG_USBHOST=1
-DCONFIG_ESP32S31_USBHOST=1
-DCONFIG_ESP32S31_USBHOST_HCD_SKELETON=1
-Werror -Wno-error=undef
```

基础 Host 配置：

```text
RC=0
object=/tmp/s31-usb-hcd-skeleton-current-base.o
text=410 data=0 bss=236 dec=646
sha256=fd4c2676cd1fdd838872454caf7174a6da05832952c1fe647e8d7b15247aa11d
```

额外启用 `CONFIG_USBHOST_HUB=1` 和 `CONFIG_USBHOST_ASYNCH=1` 以覆盖条件回调：

```text
RC=0
object=/tmp/s31-usb-hcd-skeleton-current-hub_async.o
text=442 data=0 bss=248 dec=690
sha256=a251067abab0f14f1d4d7a6c9d61108234daf99dad6ee469195f9df90480bc1f
```

未刷写、未打开串口、未接入 USB 外设。由于没有真实 channel scheduler、完成 IRQ、root-hub
 wait/enumerate、地址分配和 DMA/FIFO 传输，本轮不增加 USB2.0 或 xTS 通过数。连接回调虽已纳入 ABI 骨架，仍未接到端口状态中断和枚举 worker，调用时必然返回 `-ENOSYS`。
