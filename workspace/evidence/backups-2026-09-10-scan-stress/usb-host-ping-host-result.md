# USB HS OUT NYET / PING 修复

确认此前缺陷：HS OUT 单包收到 NYET 说明数据已被设备接收，只是下一包发送前要查询接收空间。此前统一按 EAGAIN 重发会错误处理数据完成及 toggle。

生产修复：

- 通道状态引擎区分普通包与 PING。HS OUT NYET（含 XFRC+NYET）完成当前包并保留 need_ping；NAK 不完成数据。
- need_ping 持久保存在端点，而非临时请求；下一 HS OUT 使用 HCTSIZ.DOPNG，仅发送 PING token，不写 TX FIFO。
- PING NAK 保持等待；ACK 必须等 CHH 才恢复真正数据发送。ACK 不消耗调用方数据、不推进 DATA0/1。HS 数据 NAK 先 PING 再重发原包原 PID。
- SETUP 不使用 PING并重置该控制端点握手状态；FS/LS、IN 仍沿用原行为。
- 所有阶段共用请求超时预算，STALL/硬件错误优先；未 CHH 不复用。PING 仅 XFRC 没有 ACK 返回 EPROTO，避免假定设备已经就绪。

验证：生产 PIO MMIO 模拟覆盖 NYET 不重发、跨调用 PING NAK→ACK→DATA、数据 NAK 后真正重发、XFRC+NYET、NYET+STALL 优先错误、PING STALL 和超时。测试检查 TX FIFO 写入次数，确认 PING 无数据。原根端口/EP0/边界和通道错误/过期IRQ回归继续通过 UBSan fail-fast；实际 HCD 严格交叉编译基础及 HUB+ASYNCH 全通过。

独立完整构建 `build-usb-host-ping.sh` exit0，新输出 `openvela-dev/out/esp32s31-usb-host-ping`，旧冻结固件保留、源配置恢复。固件 SHA256 `883ef65266c1467b906296e73d4fbad233d1944a479b0b7d1614c0a9e7734b9b`。日志 `usb-host-ping-host.log`、`logs/build-usb-host-ping.log`；源收据 `usb-host-ping-source.sha256`。

依据：锁定 S31 HAL `usb_dwc_ll.h` 的 `usb_dwc_ll_hctsiz_set_dopng` 明确OUT PING字段；主来源 [Linux DWC2 hcd_intr.c](https://github.com/torvalds/linux/blob/master/drivers/usb/dwc2/hcd_intr.c) 的 NYET、PING ACK 和末包 NYET 处理说明。核对使用已下载 `/tmp/s31-review-linux-dwc2-hcd_intr.c`（本轮web读取超时）；未复制其 GPL 代码。

未进行板级 USB/PIO/MSC 互通，不宣称 HS 电气/吞吐或测试通过。本轮未增加 IRQ、异步、周期或 Hub 支持。
