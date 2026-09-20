##### 5.1.15 文件系统fstest执行1000次

**测试目的：** 二、品类自测用例 > 5、性能测试 > 5.1 系统应用 > 文件系统

**前提条件：**

1、打开如下配置：

```
CONFIG_TESTING_FSTEST=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、在nsh中依次输入如下命令：
mkdir DIR/fstest
fstest -m DIR/fstest -n 1000
rm -r DIR/fstest
注：DIR为需要测试的文件系统挂载的目录，可通过df -h来查看，例如/data（默认）， /sdcard， /sst，rpmsgfs(一般非主核挂载)

**预期结果：**

1、程序正常执行,无crash,没有输出error
注：有的设备运行1000次需要2天时间

---
