# USB Host 根端口 / MSC 入口主机阶段

接续 `usb-host-pio-host-result.md`。本轮新增真实根端口入口和现有 NuttX MSC 枚举链；仍不是实物通过。

- `esp32s31_usbhost_port.h`：真实 HPRT 读取驱动连接判断，10 次 10ms 连续采样确认接入，断开/过流立即失效；50ms reset 后最多 1s 等待 enable，解码 HS/FS/LS，非法速度拒绝。每次 HPRT 写屏蔽 W1C 状态及 PENA，不伪造连接或启用。
- HCD `wait`/`enumerate` 已替换 ENOSYS，使用现有 `usbhost_devaddr_initialize`、`usbhost_enumerate`，正确配置 EP0 地址0及速度对应初始 MPS；断开通知真实 `CLASS_DISCONNECTED`。旧 class 未释放、地址未归还或端点/未停通道仍占用时禁止复用。
- 候选板级入口启动 polling worker，注册已有 `usbhost_msc_initialize`。仅隔离 Host profile 启用 MSC；生产及 Device profile 不改。
- 成功的标准 CLEAR_FEATURE(ENDPOINT_HALT) 请求重置目标 endpoint 的 DATA0，供现有 MSC 恢复路径使用。

验证：`python3 tools/tests/usbhost-pio/run.py` 的实际根端口辅助层模拟覆盖插入抖动、拔出、过流、reset timeout、HS/FS/LS、非法速度及 W1C 写入保护；原 PIO/控制数据阶段回归继续通过 UBSan fail-fast。实际 S31 HCD/初始化源文件基础及 HUB+ASYNCH 严格交叉编译通过。该模型不声称实际 class 成功绑定。

独立完整构建：`bash backups/2026-09-10-scan-stress/build-usb-host-root.sh` exit0，输出 `openvela-dev/out/esp32s31-usb-host-root`。System.map 真实链接 `esp32s31_usbhost_worker`、`esp32s31_hcd_wait/enumerate`、`usbhost_enumerate`、`usbhost_devaddr_create`、`usbhost_msc_initialize`。有效配置 USBHOST_MSC=y，USBDEV未开启。

nuttx.bin SHA256：`fc5d16e80294f8005043b9bf11cb89ff68f83094cfc563036eb2f9520e071042`。收据 `usb-host-root-source.sha256`，回归 `usb-host-root-host.log`，完整构建 `logs/build-usb-host-root.log`。源配置恢复、锁释放、未刷板或运行 MMIO。

仍待：板级插拔、实际 PIO/HS 协商、MSC 块读写及数据一致性；IRQ路由、周期/异步/Hub split 仍未支持；polling 阻塞传输期间的拔出由 PIO 检查处理，未收到 CHH 的通道继续隔离而非强行重用。旧 MSC class 因打开文件延迟销毁时新枚举会 EBUSY，应关闭旧句柄后重新插拔，不声称无缝热插拔恢复。HS ping/NYET效率、端口电气和真实设备兼容性未测。不可据此标 USB2.0 实物通过。
