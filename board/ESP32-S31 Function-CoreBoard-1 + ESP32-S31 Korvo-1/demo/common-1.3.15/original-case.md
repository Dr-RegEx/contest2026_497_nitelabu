##### 1.3.15 Watchdog测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.3 驱动BSP

**前提条件：**

1. 打开如下配置：

```
CONFIG_WATCHDOG=y
CONFIG_BOARDCTL_RESET_CAUSE=y
CONFIG_TESTING_DRIVER_TEST=y
CONFIG_CMOCKA=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口
注：芯片厂商初始化wdt时需要在wdt中断里面主动调用panic，并且提高watchdog中断优先级，保证在watchdog中断可以打断critical_section，在关中断的情况下不喂狗也可以进入wdt中断。

**步骤：**

1、在nsh中依次输入如下命令：
cmocka_driver_watchdog -r 0 //测试到达timeout后看门狗是否生效
cmocka_driver_watchdog -r 1 //测试打断critical_section，在关中断的情况下不喂狗也可以进入wdt中断。
cmocka_driver_watchdog -r 2 //测试开中断后死循环看门狗是否生效
cmocka_driver_watchdog -r 3 //测试正常喂狗，结果输出PASS
说明：执行命令cmocka_driver_watchdog，通过-r传入参数，参数为0-3，分别进入4个不同的case，所以watchdog的测试需要执行四次，参数从0到3依次执行（需按顺序执行）

**预期结果：**

1、观察-r参数为 0/1/2测试咬狗状态能否主动触发asser和打印堆栈信息并重启，并且重启原因是BOARDIOC_RESETCAUSE_SYS_RWDT，-r 参数为3时正常喂狗输出PASS

---
