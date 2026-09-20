# USB1761 S31 OTG Host HCD surface audit

日期：2026-09-19。此记录核验现有 S31 OTG Host 初始化候选和 NuttX HCD 接口边界；不代表 USB 设备枚举、端点传输或 ADB 通过。

## 离线编译结果

使用已生成的 `esp32s31-xts-flat-usb-adb/compile_commands.json` S31 RISC-V 命令，仅临时加入 `CONFIG_ESP32S31_USBHOST=1` 和 `CONFIG_USBHOST=1`，将输入替换为：

```text
openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_usbhost.c
```

并以 `-Werror` 编译，结果：

```text
USB1761_HOST_SOURCE_COMPILE_RC=0
text data bss dec hex
526 0 0 526 20e /tmp/s31-usbhost-current.o
```

同样临时打开 Host 条件编译板级 `esp32s31_bringup.c`，结果：

```text
USB1761_BOARD_BRINGUP_COMPILE_RC=0
text data bss dec hex
290 0 0 290 122 /tmp/s31-bringup-usbhost.o
```

两个对象均只写入 `/tmp`。相关 NuttX 文件的 `git diff --check` 无空白错误；没有刷写镜像、占用 UART 或改变默认配置。

## 已核验的 Host 前置

现有候选已通过锁定 S31 HAL 的 `usb_utmi_ll`/`usb_dwc_ll` 配置以下前置，并用 `_Static_assert` 固定 DWC 结构偏移：UTMI/PHY 时钟和复位、D+/D- 15k 下拉、16-bit UTMI、AHB idle 和核心 reset-done 超时、GSNPSID 校验、强制 Host mode、FS48MHz Host 时钟以及 Type-A VBUS 请求。

## 明确的 HCD 缺口

锁定 NuttX `usbhost.h` 要求 `usbhost_connection_s` 的 `wait`/`enumerate` 回调，以及 `usbhost_driver_s` 的 `ep0configure`、端点分配/释放、DMA buffer、控制/批量/中断/同步传输、异步取消、connect/disconnect 回调。当前 S31 源树没有对应实现，也没有把 DWC Host channel 的 `HCCHAR/HCTSIZ/HCDMA`、`HAINT/HAINTMSK` 和全局 IRQ 状态机接到这些回调；因此不能安全添加一个空壳 HCD 来宣称协议支持。

现有寄存器定义已经包含 Host channel 0 偏移、HPRT、HAINT 和 HCTSIZ 位域，可作为后续独立 HCD 实现的基础。下一阶段必须在隔离 Host 镜像中完成 IRQ、root-hub 连接/复位、设备地址分配、控制传输和至少一个 class driver，再接入外部 USB 设备验证枚举和端点收发。

## 结论

USB2.0 项目状态仍为“Host 初始化候选可编译，HCD/枚举待实现”。本次增加的是可复现的严格编译和接口缺口记录，不增加 xTS 4.2.2 或比赛清单的通过数。
