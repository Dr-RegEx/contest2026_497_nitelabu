##### 1.1.12 Kernel-md5值测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.1 系统内核 > 内核Kernel

**前提条件：**

1、添加任意测试文件（如1.txt）到etc目录
2、打开如下配置：

```
CONFIG_TESTS_TESTCASES
CONFIG_FS_TEST
CONFIG_FS_TEST_EDONLY
```

3、打开上述配置后进行编译
4、烧录刚刚编译的bin包
5、打开nsh窗口

**步骤：**

1、在nsh中输入 md5_test -f /etc/1.txt -c 100，1.txt为etc目录下文件
2、等待执行结果

**预期结果：**

得到100个md5值，且一致

---
