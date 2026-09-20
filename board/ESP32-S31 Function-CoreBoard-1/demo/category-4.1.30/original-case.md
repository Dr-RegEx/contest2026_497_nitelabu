##### 4.1.30 WiFi--2.4G--wapi reconncet

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > Wi-Fi

**前提条件：** 1、设备上电

**步骤：**

1、进入到NuttX shell；
2、设备配网：
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <路由器SSID> 1
renew wlan0
3、执行ifconfig，查看wlan0接口IP地址是否为essid为<路由器SSID>路由器获取IP地址
4、执行wapi save_config wlan0
5、执行wapi reconnect wlan0；
6、执行renew wlan0
7、再次执行步骤3
8、执行ping router_ip (注：router_ip为路由器网关IP)

**预期结果：**

2、设备配网成功;
3、wlan0地址为essid为<路由器SSID>路由器分配的地址
5、执行成功，无报错；
7、 wlan0地址为essid为<路由器SSID>路由器分配的地址
8、可以ping通路由器IP

---
