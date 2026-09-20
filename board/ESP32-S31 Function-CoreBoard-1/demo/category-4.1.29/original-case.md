##### 4.1.29 WiFi--2.4G--wapi config保存后设备reboot再reconnect

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > Wi-Fi

**前提条件：** 1、设备上电

**步骤：**

1、设备进入到NuttX shell；
2、设备配网：
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <路由器SSID> 1
renew wlan0
3、执行wapi save_config wlan0；
4、 执行reboot；
5、执行 ifup wlan0
6、执行wapi reconnect wlan0
7、执行renew wlan0
8、执行ping router_ip (注：router_ip为路由器网关IP)

**预期结果：**

1-7、执行成功无报错；
8、设备重新配网成功，可以ping通路由器IP

---
