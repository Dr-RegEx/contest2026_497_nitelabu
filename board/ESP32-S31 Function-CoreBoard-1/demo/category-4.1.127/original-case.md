##### 4.1.127 Audio--nxlooper回环功能测试

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > Audio

**前提条件：**

1、打开如下配置：

```
CONFIG_SYSTEM_NXRECORDER=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、在nsh中执行如下命令：
nxlooper
device pcm0p
device pcm13c
loopback 2 16 48000
stop
q

**预期结果：**

1、进入nxlooper后执行命令无报错

---
