##### 4.1.21 WiFi--2.4G--Wapi连接为WPA2加密方式时加入2.4G网络

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > Wi-Fi

**前提条件：** 1、设备上电，2.4G网络无线AP设置加密方式为WPA2模式

**步骤：**

1、设备进入到NuttX shell
2、设备配网：
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <路由器密码> 3
wapi essid wlan0 <路由器SSID> 1
renew wlan0
3、执行ifconfig；
4、执行ping router_ip（注：router_ip为路由器IP）

**预期结果：**

1-2、执行成功无报错，设备成功配网；
3、设备分配到ip地址；
4、可以ping通路由器IP，且无异常报错

---
