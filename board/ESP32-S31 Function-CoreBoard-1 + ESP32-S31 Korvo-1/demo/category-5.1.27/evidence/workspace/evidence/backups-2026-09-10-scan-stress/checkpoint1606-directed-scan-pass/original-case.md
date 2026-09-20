##### 5.1.27 WiFi--2.4G网络---设备连接2.4G后wapi扫描指定2.4G AP

**测试目的：** 二、品类自测用例 > 5、性能测试 > 5.1 系统应用 > Wi-Fi

**前提条件：** 1、设备上电

**步骤：**

1、设备进入到NuttX shell；
2、设备配网:
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <路由器SSID> 1
renew wlan0
3、执行wapi scan wlan0 <指定2.4G ssid>；
4、重复步骤3执行100次，查看返回结果是否有变化，串口log是否有扫描失败情况或扫描异常报错

**预期结果：**

1-3、执行成功无报错，可搜索到指定的2.4G SSID
4、返回结果不会产生变化，扫描列表无异常无报错

---
