# USB Device / ADB S31 UTMI 核查

核查现有 `esp32s31_usbdev.c` 的 S31 DWC 布局、初始化和枚举完成设置；未访问 MMIO、串口或设备。

发现并修复一个明确规格不匹配：`esp32s31_enuminterrupt()` 原写 `USBTrdTim=6`，为 S3 内部 FS PHY 遗留值。S31 已使用 `usb_dwc_ll_gusbcfg_set_utmi_phy` 设置 16-bit UTMI+；锁定 `components/soc/esp32s31/register/soc/usb_otghs_struct.h:536` 明确要求 16-bit UTMI+ 的 turnaround 为5，8-bit为9。改为5并修正注释。此为寄存器时序规格修正，未证明它是实物枚举失败的唯一原因。

已核对无需重复修改的部分：

- 独立UTMI时钟、复位、去suspend、16bit PHY选择，timeout calibration=5。
- S31 core reset使用bit29 RESET_DONE，显式清CSRST并ack RESET_DONE；AHB idle和reset等待均有超时。
- Force Device、保持soft-disconnect至CLASS_BIND、无VBUS sensing时B-session有效override字段与锁定结构一致。
- DCFG使用FS-on-HS编码1，匹配当前FS-only Device实现；此候选并非480Mbps Device。
- FIFO按32bit words，EP0低16位start/high16位depth，其他IN endpoint相同；运行时检查core ID、端点能力、动态FIFO和容量。现有320word预算是FS保守分区，不代表S31真实总FIFO容量仅320word。
- 现有编译断言校验reset、DCFG、EP0 IN/OUT、PCGCCTL实际结构偏移。

实际 Device/ADB 源严格交叉编译已通过，冻结Device配置优先include，未把Host配置混入。无须针对常量修改另造模拟测试。

仍需实物：USB设备口电气/安全连接条件、实际主机枚举、ADB传输。板上Type-A是Host用途，不能从此代码核查推断可直接用不适当连接作为Device。HS Device描述符/512B bulk尚未实现，本轮不扩该目标。

完整隔离Device/ADB构建 `build-usb-device-utmi.sh` exit0，输出 `openvela-dev/out/esp32s31-usb-device-utmi`。固件SHA256 `372f9231fdb887767ff74e3b56776dd8a06c60973f389c155f916f142a84436c`。旧镜像未覆盖，源配置已恢复；编译日志 `usb-device-utmi-host.log`，完整构建 `logs/build-usb-device-utmi.log`，收据 `usb-device-utmi-source.sha256`。
