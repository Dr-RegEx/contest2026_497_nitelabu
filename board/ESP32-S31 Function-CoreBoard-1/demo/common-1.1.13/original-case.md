##### 1.1.13 Kernel-C++功能测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.1 系统内核 > 内核Kernel

**前提条件：**

1、打开如下配置：

```
CONFIG_HAVE_CXX=y
CONFIG_EXAMPLES_HELLOXX=y
CONFIG_TESTING_CXXTEST=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、在nsh中输入 cxxtest
2、等待执行结果

**预期结果：**

测试完成后，nsh终端打印
Test std::vector ============================
v1=1 2 3
s1=Hello, World!
Hello World Good Luck
Test std::map ============================
Test RTTI ============================
extend

---
