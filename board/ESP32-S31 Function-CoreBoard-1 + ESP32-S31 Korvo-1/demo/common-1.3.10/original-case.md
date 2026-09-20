##### 1.3.10 Uart串口功能测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.3 驱动BSP

**前提条件：**

1、打开如下配置：

```
CONFIG_TESTING_CMOCKA=y
CONFIG_TESTING_DRIVER_TEST=y
CONFIG_SERIAL=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、使用串口小板连接设备和PC：
USB2UART Host
TX -- RX
RX -- TX

**步骤：**

优先用手动测试，脚本存在问题需要修正
| 手动测试
1、在nsh中输入 ls /dev，以查询设备名称，如ttyS0
2、在nsh中输入：cmocka_driver_uart -d /dev/ttyS0
3、在串口粘贴复制如下内容并回车
0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ,./<>?;':"[]{}\|!@/#$%^&/*()-+_=
4、输入0 然后回车
5、输入/#
6、查看是否测试PASS
| 脚本测试
1、在插入了USB转UART模块的电脑上，在/dev目录中找到对应的tty设备：ls /dev/ttyUSB/* ，/*为数字
2、在电脑终端执行：
sudo python3 testing/drivertest/test_content_gen.py /dev/ttyUSB/*（/*根据实际情况填写）
3、在nsh中输入 ls /dev，以查询设备名称，如ttyS4
4、继续在nsh中输入 cmocka_driver_uart -d /dev/ttyS4，等待执行结果

**预期结果：**

测试结果PASS，无异常

---
