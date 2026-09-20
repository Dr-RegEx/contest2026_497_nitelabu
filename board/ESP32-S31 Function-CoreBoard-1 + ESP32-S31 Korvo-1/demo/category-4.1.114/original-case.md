##### 4.1.114 NetApp--curl网页能力测试

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > NetApp

**前提条件：**

1、打开如下配置：

```
CONFIG_LIB_ZLIB=y
CONFIG_CRYPTO_MBEDTLS=y
CONFIG_LIB_CURL=y
CONFIG_TOOLS_CURL=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、设备进入到NuttX shell
2、设备配网：
ifup wlna0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <热点名称> 1
renew wlan0 ；
3、执行命令 curl www.baidu.com

**预期结果：**

2、设备配网成功
3、 网页内容打印到串口中

---
