# USB Host DWC2 通道 IRQ/停止状态：主机适配阶段

日期：2026-09-19。清单用户自添加第12项，限主机工作；未刷板、未打开串口、未执行硬件MMIO。

## 实际实现

新增 `esp32s31_usbhost_channel.h` 通道状态引擎，由现有HCD中的真实寄存器访问器及 `esp32s31_usbhost_hcd_channel_interrupt` 调用，不是返回成功的传输stub。HCD按HAINT/HAINTMSK分发，读取HCINT/HCINTMSK/HCTSIZ，W1C清理观测到且启用的状态，必要时设置HCCHAR CHENA+CHDIS请求停止。通道数量从骨架8个纠正为锁定S31 HAL真实16个。

行为：

- XFRC依据剩余长度计算实际完成字节，等CHH后才进入DONE；同时XFRC/CHH可以完成，不能把ACK当成功。
- STALL→EPIPE、AHB错误→EIO、babble→EOVERFLOW、transaction/toggle错误→EPROTO、frame overrun→EXDEV。
- NAK/NYET→EAGAIN交还未来调度器重试，不报告传输成功；未实现自动重试调度。
- 取消/超时记录ECANCELED/ETIMEDOUT并请求halt，等待真实CHH。迟到XFRC不能覆盖取消/超时；完成候选后出现STALL也不能被隐藏。
- ACTIVE/STOPPING或硬件CHENA仍置位时禁止arm复用；可复用时先屏蔽、清旧通道状态再启用，迟到/重复IRQ不重复完成。
- 未知保留状态不被盲目W1C清除。S31 HAL明确HCINT bit2为AHBERR，本阶段未沿用旧STM32 FS注释中“reserved”的错误假设。

新增arm/stop接口及HCD通道IRQ入口对临界区正确加锁。arm仅准备状态/中断，尚不启动数据：未来launcher必须先配置数据/FIFO/DMA/HCTSIZ，再启动HCCHAR并接通全局IRQ。获取原HCD骨架或执行原contract不触碰MMIO。

## 验证

运行 `python3 tools/tests/usbhost-channel/run.py`：

- 使用同一生产状态引擎的模拟寄存器验证短包完成、XFRC与CHH同时/分开、八类错误/重试、错误优先级、取消与超时等CHH、迟到完成、重复IRQ、复用前清状态、W1C范围和参数边界；UBSan fail-fast通过。
- 使用锁定S31实际compile_commands严格交叉编译HCD源：基础、HUB+ASYNCH两种条件组合均通过，`-Werror`（既有undef警告例外）。
- 日志：`usb-channel-state-host.log`。
- 源文件哈希：`usb-channel-state-source.sha256`。

未全量构建新镜像，不占用源树配置锁。

## 尚未完成

IRQ入口尚未attach到板级中断，也未连waiter/callback通知；endpoint分配、scheduler、FIFO/DMA数据移动、root-hub热插拔和枚举仍待实现。原公开NuttX传输回调继续返回ENOSYS，因此没有把阶段代码伪装成可用USB Host，更没有新增USB2.0/xTS通过项。必须后续接入真实USB外设测试才能判定硬件行为。
