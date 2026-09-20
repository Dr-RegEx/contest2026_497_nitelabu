##### 2.1.4 Reboot启动时间测试

**测试目的：**  验证系统设备reboot系统启动时长符合OS标准

**前提条件：** 1、 测试设备串口正常 

**步骤：**

1、在nsh中输入 reboot 10次，统计minicom串口工具时间戳reboot开始，到系统启动完成对应的的关键字‘NuttShell (NSH)’的平均启动时长
 注：
1）打开minicom时间戳打印：ctrl +A +Z +N
2）整理log竖排打印：ctrl +A +Z+U

**预期结果：**

1、设备reboot 10次平均时长不超过6秒 

---
