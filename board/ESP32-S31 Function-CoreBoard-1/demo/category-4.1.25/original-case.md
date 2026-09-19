##### 4.1.25 WiFi--2.4G--Wapi Sense获取链接2.4G AP的RSSI

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
3、执行wapi sense wlan0
4、再执行步骤3，查看返回结果是否有变化

**预期结果：**

3、执行成功无报错，返回rssi信息
4、两次返回结果会动态变化

---
