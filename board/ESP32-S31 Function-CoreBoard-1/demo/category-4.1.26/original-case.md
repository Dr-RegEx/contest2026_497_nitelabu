##### 4.1.26 WiFi--2.4G--Wapi Show获取2.4G AP的无线参数

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > Wi-Fi

**前提条件：** 1、设备上电

**步骤：**

1、设备进入到NuttX shell
2、设备配网：
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <路由器SSID> 1
renew wlan0
3、执行wapi show wlan0

**预期结果：**

1-2、执行成功无报错，可查看wlan0接口信息，包括：IP、 NetMask、Frequency、Flag、Channel、Frequency、ESSID等

---
