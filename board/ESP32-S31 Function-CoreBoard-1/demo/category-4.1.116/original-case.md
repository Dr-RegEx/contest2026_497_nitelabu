##### 4.1.116 NetApp--设备支持通过scp方式向其他终端传输本地文件

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > NetApp

**前提条件：**

1、设备上电，配网在线，且与对端PC在同一局域网
2、打开如下配置：

```
CONFIG_DEV_URANDOM=y
CONFIG_CRYPTO_MBEDTLS=y
CONFIG_LIB_SSH=y
CONFIG_UTILS_SSH=y
```

3、打开上述配置后进行编译
4、烧录刚刚编译的bin包
5、打开nsh窗口

**步骤：**

1、设备进入NuttX shell进行配网:
执行：
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <热点名称> 1
renew wlan0
2、执行scp /data/<本地文件> <终端用户名@HostIP:文件所在绝对路径>
eg:scp /data/full_ota.zip <user>@<file_server_ip>:/home/sss/Downloads/
3、输入 yes, 回车, 回车, 输密码
eg:
scp /data/full_ota.zip <user>@<file_server_ip>:/home/sss/Downloads/
4、等待数据传输成功后，在终端上查看文件大小是否和设备上的本地的文件大小相同

**预期结果：**

1、设备配网成功
2-3、传输无报错
4、终端上的文件大小和设备上的本地的文件大小相同

---
