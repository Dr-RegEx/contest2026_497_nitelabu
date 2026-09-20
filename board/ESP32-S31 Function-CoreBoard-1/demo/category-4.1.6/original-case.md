##### 4.1.6 文件系统异常场景--掉电重启时磁盘空间变化

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.1 系统应用 > 文件系统

**前提条件：**

1、打开如下配置：

```
CONFIG_TESTS_TESTCASES=y
CONFIG_TESTING_TESTCASES_STACKSIZE=8192
CONFIG_FS_TEST=y
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

1、在nsh中输入 power_off_test03 <DIR>
2、执行一段时间，将设备直接断电
3、恢复设备供电，等设备起来后在nsh中输入 power_off_test03 <DIR>
4、重复步骤2-3 3次，观察测试结果
注：DIR为需要测试的文件系统挂载的目录，可通过df -h来查看，例如/data（默认）， /sdcard， /sst，rpmsgfs(一般非主核挂载)

**预期结果：**

4、输出TEST PASSED

---
