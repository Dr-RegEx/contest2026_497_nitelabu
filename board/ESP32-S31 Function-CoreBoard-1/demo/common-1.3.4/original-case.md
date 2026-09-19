##### 1.3.4 RAM随机读写测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.3 驱动BSP

**前提条件：**

1、打开如下配置：

```
CONFIG_TESTING_CMOCKA
CONFIG_TESTING_DRIVER_TEST
CONFIG_TESTING_DRIVER_TEST_STACKSIZE=8192
CONFIG_BCH
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口
5、在测试平台/dev下可以找到RAM对应的设备名称：ls /dev

**步骤：**

1、在nsh中输入以下命令：
mkrd -m 10 -s 1000 1024
cmocka_driver_block -m /dev/dev_name，其中dev_name根据实际的设备名称来填写
2、等待执行结果

**预期结果：**

测试结果PASS，无异常

---
