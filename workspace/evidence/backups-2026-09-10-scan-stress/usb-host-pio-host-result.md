# USB Host EP0 / bulk PIO 主机阶段记录

本记录只证明真实生产代码的主机模拟、S31 严格编译和隔离固件链接，不证明 USB 实物枚举或 xTS 通过。

## 已实现

- HCD endpoint 分配/释放、EP0 配置、传输缓冲分配释放，支持 FS/LS control、FS bulk、HS EP0 64 字节及 bulk 512 字节。
- 同步 control 使用 SETUP(DATA PID=SETUP)、DATA1 起始数据阶段和反向 DATA1 零长度 STATUS；无数据请求 STATUS 固定 IN。bulk 保留成功包后的 DATA0/1 toggle；IN 短包结束。
- 真实 S31 MMIO FIFO PIO 访问通过共用锁串行化；逐包 CPU 打包/解包，尾部不越界，RX 过量安全排空并报错。NAK/NYET 有界重试；STALL、传输错误、断开和超时返回真实错误。通道未确认 CHH 停止禁止复用。
- OUT slave 模式可能不递减 HCTSIZ：以无错误 XFRC+CHH 认定整包成功，不以 remaining=0 为条件；IN 核对实际 FIFO 数据和剩余计数。
- 初始化使用 S31 HS UTMI 时钟选择，关闭 descriptor DMA、关闭 GAHBCFG DMA 并读回；检查动态 FIFO 和 16 通道能力，检查 FIFO 至少 768 words，配置 RX512 / nonperiodic TX256 words。
- HPRT 加 PPWR 前屏蔽 W1C 状态位及 PENA，避免误清状态/误禁用端口。

## 主机结果

`python3 tools/tests/usbhost-pio/run.py` 通过 UBSan fail-fast：非对齐 TX、尾字节、IN 短包/溢出、NAK 重试、STALL、超时未 CHH 禁止复用、断开/DMA模式拒绝、EP0 正常/短包/失败阶段及 HS 512 字节分包。OUT 模拟保留整个 HCTSIZ 原值；不是人为置零来制造通过。

同一命令严格交叉编译实际 usbhost.c / hcd_skeleton.c，各覆盖基础和 HUB+ASYNCH 配置。日志：`usb-host-pio-host.log`。

完整隔离 profile `xts-flat-usb-host-pio` 构建成功：`openvela-dev/out/esp32s31-usb-host-pio`。`USBHOST`、S31 Host、HCD candidate 均 y，USBDEV 未启用；System.map 包含 `s31_usb_pio_packet`、`esp32s31_hcd_epalloc/control/transfer`、`g_pio_lock`。生产 profile 和旧固件未覆盖，临时源配置已恢复。

固件 SHA256：`5de5a78dee105a3829fc05d6cccee7769c9d28d267ba93534e7a98515f899367`。完整源/配置收据：`usb-host-pio-source.sha256`。构建脚本/日志：`build-usb-host-pio.sh`、`logs/build-usb-host-pio.log`。

## 依据与边界

S31 锁定 HAL `components/esp_hal_usb/esp32s31/include/hal/usb_dwc_ll.h` 提供真实 `gahbcfg_en_slave_mode` 和 FIFO 分区接口；同目录库的 `usb_dwc_hal.c` 仅在 hsphy_type==0 配置 FS48/6MHz，支持此处 UTMI 选择修正。S31 `usb_dwc_struct.h` 用于寄存器布局编译断言。以上不代表已读取实际 GHWCFG2 或验证本硅片 slave 模式。

OUT residual 行为交叉核验官方 [Linux DWC2 hcd_intr.c](https://github.com/torvalds/linux/blob/master/drivers/usb/dwc2/hcd_intr.c) 中 `dwc2_get_actual_xfer_length`：IN 用剩余量，普通成功 OUT 用请求长度；未复制 GPL 实现。

仍未完成：root-hub reset/连接监控/枚举入口、IRQ 路由、periodic/interrupt/isochronous、异步取消、hub split、HS ping/NYET 吞吐策略和 clear-halt 后 toggle 复位集成。当前有界同步原语不等于完整可用 HCD。无 USB 外设/板级 MMIO/刷写测试；HS 实际协商、FIFO PIO 时序、传输可靠性及速率都待实物验证，不能将主机模拟标为 USB2.0 通过。
