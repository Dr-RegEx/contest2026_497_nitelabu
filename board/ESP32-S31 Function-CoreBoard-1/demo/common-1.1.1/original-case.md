##### 1.1.1 系统内存管理测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.1 系统内核 > 内存管理

**前提条件：**

1、打开如下配置：

```
CONFIG_TESTING_CMOCKA=y
CONFIG_TESTS_TESTSUITES=y
CONFIG_TESTS_TESTSUITES_STACKSIZE=16384
CONFIG_CM_MM_TEST=y
+CONFIG_ARCH_SETJMP_H=y
+CONFIG_BUILTIN=y
+CONFIG_NSH_BUILTIN_APPS=y
+CONFIG_SCHED_HAVE_PARENT=y
+CONFIG_SCHED_LPWORK=y
```

2、打开后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、在nsh>中输入 cmocka_mm_test
2、等待执行结果

**预期结果：**

测试结果PASS，无异常

---
