##### 4.1.31 WiFi--2.4G--wapi disconncet后与外网不通

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > Wi-Fi

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
4、ping <外网ip地址> ，如www.baidu.com
5、执行wapi disconnect wlan0
6、ping <外网ip地址> ，如www.baidu.com

**预期结果：**

2、设备配网成功；
3、可查看到IP地址，该IP地址为路由器分配的ip；
4、执行成功；
5、ping不通，系统无异常

---
