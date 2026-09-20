##### 5.1.16 文件系统碎片读写测试vela_fs_file_fragmentation_test

**测试目的：** 二、品类自测用例 > 5、性能测试 > 5.1 系统应用 > 文件系统

**前提条件：**

1、打开如下配置

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

1、在nsh中依次输入如下命令：
mkdir /DIR/fstest
vela_fs_file_fragmentation_test -d /DIR/fstest -n 1000 -s 20，其中参数说明如下：
-d: set test dir path. -- 测试运行的目录
-n: The number of small files created in the test. -- 测试中产生的小文件数量（每个小文件的大小随机产生，最大不超过128字节）
-s: Large file size created in test. (unit:M)\n") -- 测试中产生的大文件大小上限，单位M
注：
1）测试中根据芯片资源的实际情况，合理填写 -n 和 -s参数
2）DIR为需要测试的文件系统挂载的目录，可通过df -h来查看，例如/data（默认）， /sdcard， /sst，rpmsgfs(一般非主核挂载)

**预期结果：**

1、运行一段时间后无异常

---
