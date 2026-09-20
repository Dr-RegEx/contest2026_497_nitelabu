##### 4.1.117 NetApp--设备支持ftpd

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > NetApp

**前提条件：**

1、设备上电，且配网在线
2、打开如下配置文件：

```
CONFIG_EXAMPLES_FTPD=y
CONFIG_NETUTILS_FTPD=y
CONFIG_NET_TCPBACKLOG=y
```

3、打开上述配置后进行编译
4、烧录刚刚编译的bin包
5、打开nsh窗口

**步骤：**

1、设备端执行：ftpd_start -4 &
2、同一局域网对端设备PC执行：ftp <设备IP>
输入用户名：
password:
3、PC 端进入data目录
cd data
ls
1）get <设备data目录下的某个文件> <文件在pc上想要保存的名称>
2）put <pc 当前目录下某个文件> <文件在设备上想要保存的名称>
4、PC 执行quit 退出ftp

**预期结果：**

2、PC端可以正常登录设备的ftp；
3、传输文件正常，无报错，设备端log无异常；
1）设备上的文件通过get拉取到了pc上；
2）pc上的文件通过put传输到了设备中
4、PC正常退出，设备端串口log无异常

---
