##### 1.1.10 Kernel-popen测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.1 系统内核 > 内核Kernel

**前提条件：**

1、打开如下配置：

```
CONFIG_SYSTEM_POPEN=y
CONFIG_EXAMPLES_POPEN=y
CONFIG_DISABLE_POSIX_TIMERS=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、在nsh中输入popen
2、等待执行结果

**预期结果：**

测试完成后，nsh终端打印Calling pclose()

---
