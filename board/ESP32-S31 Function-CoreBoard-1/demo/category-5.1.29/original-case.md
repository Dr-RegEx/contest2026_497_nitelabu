##### 5.1.29 WiFi--2.4G网络---wapi反复disconnect 2.4G网络

**测试目的：** 二、品类自测用例 > 5、性能测试 > 5.1 系统应用 > Wi-Fi

**前提条件：** 1、设备上电

**步骤：**

1、设备进入到NuttX shell
2、设备配网：
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <路由器SSID> 1
renew wlan0 ；
3、执行ifconfig,查看wlan0的ip;
4、执行wapi disconnect wlan0；
5、ping <wlan0 接口IP地址>
6、再次配网
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <路由器SSID>1
renew wlan0 ；
7、执行ping router_ip (注：router_ip为路由器网关IP)
8、重复步骤3-7 100次

**预期结果：**

2、设备配网成功；
3、可查看到IP地址，该IP地址为路由器分配的ip
4、执行成功，无报错；
5、不可以ping通
6、再次配网成功无报错
7、可以ping通外网
8、重复100次全部成功无报错

---
