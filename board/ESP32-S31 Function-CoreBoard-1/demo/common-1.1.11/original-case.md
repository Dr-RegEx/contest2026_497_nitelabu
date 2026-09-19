##### 1.1.11 Kernel-pipe测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.1 系统内核 > 内核Kernel

**前提条件：**

1、打开如下配置：

```
CONFIG_PIPES=y
CONFIG_EXAMPLES_PIPE=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、在nsh中输入以下命令：
pipe
rm /var/testfifo-1
rm /var/testfifo-1
2、等待执行结果

**预期结果：**

执行pipe指令测试完成后，nsh终端打印Returning success

---
