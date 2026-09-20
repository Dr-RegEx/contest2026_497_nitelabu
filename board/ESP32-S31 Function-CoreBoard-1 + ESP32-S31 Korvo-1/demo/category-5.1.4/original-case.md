##### 5.1.4 文件系统反复创建和删除文件vela_fs_stress_maximum_file_name_test

**测试目的：** 二、品类自测用例 > 5、性能测试 > 5.1 系统应用 > 文件系统

**前提条件：**

1、打开如下配置：

```
CONFIG_FS_TEST=y
CONFIG_TESTS_TESTCASES=y
CONFIG_TESTING_TESTCASES_STACKSIZE=8192
CONFIG_FS_TEST_FUNCTION=y
CONFIG_FS_TEST_STRESS=y
CONFIG_FS_TEST_STABILITY=y
CONFIG_FS_TEST_POWEROFF=y
CONFIG_ARCH_SETJMP_H=y
CONFIG_PSEUDOFS_SOFTLINKS=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、在nsh中输入vela_fs_stress_maximum_file_name_test <DIR>
注：DIR为需要测试的文件系统挂载的目录，可通过df -h来查看，例如/data（默认）， /sdcard， /sst，rpmsgfs(一般非主核挂载)

**预期结果：**

1、输出TEST PASSED
注：
1）对于有的文件系统如fatfs，该case可能会运行失败；但只要不crash，且文件系统还可以正常使用就可认为符合预期
2）测试产生的文件无法正常删除，只能通过umount，再mount --forceformat 重新挂载文件系统来删除)

---
