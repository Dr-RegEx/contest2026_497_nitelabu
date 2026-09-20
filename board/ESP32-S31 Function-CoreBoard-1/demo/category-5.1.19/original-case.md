##### 5.1.19 文件系统crash后文件完整性校验vela_fs_stability_test04 文件

**测试目的：** 二、品类自测用例 > 5、性能测试 > 5.1 系统应用 > 文件系统

**前提条件：**

1、打开如下配置：

```
CONFIG_FS_TEST=y
CONFIG_TESTS_TESTCASES=y
CONFIG_TESTING_TESTCASES_STACKSIZE=32768
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

1、执行vela_fs_stability_test04 [DIR]
2、等待10-20秒后手动触发板子重启（retset按键\手动触发crash\断电重启均可）
3、系统完成重启后再次执行vela_fs_stability_test04 [DIR]
4. 用例会检查重启后文件系统分区是否正常，之前写入的文件是否正常（写入的内容无丢失，文件可再次读写）
注：DIR为需要测试的文件系统挂载的目录，可通过df -h来查看，例如/data（默认）， /sdcard， /sst，rpmsgfs(一般非主核挂载)

**预期结果：**

1、测试成功会输出“TEST PASSED”

---
