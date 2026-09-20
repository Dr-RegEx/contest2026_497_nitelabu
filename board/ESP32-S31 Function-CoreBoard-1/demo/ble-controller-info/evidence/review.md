# BLE控制器版本：实板读取结果

2026-09-19，1747内核/AppFS配对刷写并校验通过，准备临时BLE目录后实际启动bttool、enable。控制器版本日志：

```text
S31 BLE controller HCI=6.0(0x0e) rev=0x0000 mfr=0x02e5 LEfeat=ff79ffff901b0000
```

原始UART包含Read Local Version Information（0x1001）和LE Read Local Supported Features（0x2003）成功完成。ZBlue `read_local_ver_complete` 从控制器回包写入 `hdev->hci_version`，不是host默认版本宏。初始化result=0；采集后disable/quit返回NSH。

此前静态库字符串 `ble52_controller_sdk` 不能代表本次运行控制器的HCI版本，“当前控制器5.2”推断由本次运行证据纠正。此结果证明实际报告版本不低于5.4，不能单独证明5.4全部能力、协议认证或所有xTS互通测试通过；BLE用例及LE Audio/Mesh各自保留未验证范围。

- [完整UART](ble-controller-resume-live/uart.log)
- [结果及配对固件哈希](ble-controller-resume-live/result.json)
- [刷写日志](ble-controller-resume-flash.log)
- [复现脚本](ble1747-controller-info-capture.py)，后续运行用`--output`指定新证据目录。
