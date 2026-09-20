#### 2.1 系统应用

##### 2.1.3 Cold Boot启动时间测试

**测试目的：**  验证系统cold boot时长符合OS标准

**前提条件：** 1、 测试设备串口正常 

**步骤：**

1、将设备上下电cold boot 10次，统计minicom串口工具时间戳开始，到系统启动完成对应的的关键字‘NuttShell (NSH)’的平均启动时长
 注：
1）打开minicom时间戳打印：ctrl +A +Z +N
2）整理log竖排打印：ctrl +A +U

**预期结果：**

1、设备cold boot 10次平均时长不超过4秒 
