##### 4.1.57 WiFi--wapi CountryCode的设置和获取

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > Wi-Fi

**前提条件：** 1、设备上电

**步骤：**

1、设备进入到NuttX shell；
2、执行:
ifup wlan0
wapi country wlan0
wapi country wlan0 US
wapi country wlan0
wapi country wlan0 CN
wapi country wlan0

**预期结果：**

2、执行成功无报错，可正确设置和获取country code:

---
