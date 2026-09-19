##### 4.1.3 fatutf8文件系统编码能力测试

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > 文件系统

**前提条件：**

1、打开如下配置：

```
CONFIG_TESTING_FATUTF8=y
CONFIG_TESTING_FATUTF8_STACKSIZE=3072
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、在nsh中输入fatutf8 测试路径，如fatutf8 /data
2、等待执行结果

**预期结果：**

removed testdir

---
