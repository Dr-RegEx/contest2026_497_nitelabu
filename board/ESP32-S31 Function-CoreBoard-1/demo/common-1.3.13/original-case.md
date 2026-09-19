##### 1.3.13 Timer定时器功能测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.3 驱动BSP

**前提条件：**

Timer作为系统时钟分为arch alarm和arch timer两种，需判断厂商适配的是哪种timer，执行ls /dev，看下被注册的是哪种设备节点，arch alarm为/dev/oneshot，arch timer为/dev/timer
1、打开如下配置：

```
CONFIG_TESTING_DRIVER_TEST=y
CONFIG_TESTING_CMOCKA=y
CONFIG_ONESHOT=y
CONFIG_ALARM_ARCH=y
CONFIG_TIMER=y
CONFIG_TIMER_ARCH=y
```

1）若厂商适配arch alarm，需打开
2）若厂商适配arch timer，需打开
2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

若厂商适配arch alarm
1、在测试平台/dev下可以找到oneshot对应的设备名称：ls /dev
2、在nsh中输入 cmocka_driver_oneshot -d /dev/oneshot/*，/*为数字
3、等待执行结果
若厂商适配arch timer
4、在测试平台/dev下可以找到oneshot对应的设备名称：ls /dev
5、在nsh中输入 cmocka_driver_oneshot -d /timer/*，/*为数字
6、等待执行结果

**预期结果：**

3、默认延迟25s后输出测试结果，测试结果PASS且无异常
6、默认延迟20s后输出测试结果，测试结果PASS且无异常

---
