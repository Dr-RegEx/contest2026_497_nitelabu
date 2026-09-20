##### 1.3.12 RTC时钟功能测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.3 驱动BSP

**前提条件：**

1、打开如下配置：

```
CONFIG_RTC=y
CONFIG_RTC_DRIVER=y
CONFIG_RTC_ARCH=y
CONFIG_RTC_PERIODIC=y
CONFIG_RTC_IOCTL=y
CONFIG_TESTING_DRIVER_TEST=y
CONFIG_TESTING_CMOCKA=y
CONFIG_SIG_EVTHREAD=y
CONFIG_RTC_ALARM=y
CONFIG_DRIVERS_RTC=y
CONFIG_RTC_DATETIME=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、在nsh中输入 cmocka_driver_rtc
2、等待执行结果

**预期结果：**

测试结果PASS，无异常

---
