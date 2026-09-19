# USB1776 S31 Host register layout guard

日期：2026-09-19。此记录是 USB2.0 用户自添加第 12 项的离线边界增量，不代表 HCD、设备枚举、端点传输或 xTS 4.2.2 通过。

## 本轮改动

在 `esp32s31_otg.h` 中补齐 DWC Host channel 的 HCDMA/HCDMAB 偏移宏，并修正 HCTSIZ 偏移注释。`esp32s31_usbhost.c` 的 Host 候选增加编译期 `offsetof` 护栏，覆盖：

- Host 全局 HAINT、HAINTMSK、HPRT；
- 通道 0 的 HCINT、HCINTMSK、HCTSIZ；
- 通道 0 的 HCDMA、HCDMAB。

这些断言只在显式打开 `CONFIG_ESP32S31_USBHOST` 时编译，不改变默认生产、xTS 或 Device/ADB 配置。它们把后续 HCD 实现所依赖的 DWC 结构布局锁定，避免寄存器映射漂移被误当成协议适配。

## 离线验证

以已生成的 `openvela-dev/out/esp32s31-xts-flat-usb-adb/compile_commands.json` 中 S31 RISC-V 命令为基础，临时加入 `CONFIG_USBHOST=1`、`CONFIG_ESP32S31_USBHOST=1` 和 `-Werror -Wno-error=undef`，将 Host 源编译到 `/tmp`：

```text
RC=0
object=/tmp/s31-usbhost1776.o
text=526 data=0 bss=0 dec=526 hex=20e
sha256=0c088ca4488afc5e4cd2d1e2627e5fd187e2d9b0f0d653d7c5306f84e1c98929
```

`-Wundef` 的锁定 SDK 配置宏缺失仅保留为 warning，其他诊断按 `-Werror` 处理；静态断言全部通过。相关两个源文件无行尾空白错误，未刷写镜像、未打开串口、未接入外部 USB 设备。

## 仍未闭合

重新运行 `usb1763-hcd-api-audit.sh` 仍得到 14 个必需 NuttX Host 回调缺失，Host IRQ、channel scheduler、root-hub wait/enumerate、设备地址、class driver 和端点传输均未实现。因此本轮只增加寄存器布局护栏，不增加 USB2.0 或 xTS 通过数；恢复人工和外设条件后，仍需按 `sd-usb1753-hardware-runbook.md` 完成 HCD 与真实枚举验证。
