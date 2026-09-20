##### 5.1.13 裸设备读写性能测试

**测试目的：** 二、品类自测用例 > 5、性能测试 > 5.1 系统应用 > 文件系统

**前提条件：**

1、打开如下配置：

```
CONFIG_NSH_CMDOPT_DD_STATS=y
CONFIG_DEV_NULL=y
CONFIG_DEV_ZERO=y
CONFIG_NSH_DISABLE_DD=n
```

2、关闭如下配置：
3、打开上述配置后进行编译
4、烧录刚刚编译的bin包
5、打开nsh窗口

**步骤：**

1、查找待测试设备名称，例如 /dev/data
2、在nsh中输入 dd if=/dev/zero of=/dev/data bs=blocksize count=count (顺序写)
3、在nsh中输入 dd if=/dev/data of=/dev/null bs=blocksize count=count (顺序读)

**预期结果：**

2/3、测试无异常，输出读写速度

---
