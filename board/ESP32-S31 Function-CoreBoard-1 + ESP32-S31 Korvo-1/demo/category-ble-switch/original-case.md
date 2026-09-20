##### 开关BLE蓝牙

**测试目的：** 开关BLE蓝牙

**前提条件：** 切换到bt所在核： 1. bttool 

**步骤：**

1、测试设备执行enable
2、测试设备执行state
3、测试设备执行disable
4、测试设备执行state

**预期结果：**

1、命令执行成功，回调Adapter state changed: 2
2、命令执行成功，回调[bttool] Adapter State: 2
3、命令执行成功，回调Adapter state changed: 0
4、命令执行成功，回调[bttool] Adapter State: 0

---
