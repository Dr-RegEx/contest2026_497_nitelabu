##### 4.1.28 WiFi--2.4G--wapi config保存正常

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
4、执行cat /data/etc/wapi.conf，查看成功保存的config信息是否准确，有无乱码显示异常
注：不同设备保存wapi.conf的路劲不一，以实际为准

**预期结果：**

2-3、执行成功无报错，设备成功配网
4、可查看成功保存的wapi.conf，信息准确，无乱码显示异常

---
