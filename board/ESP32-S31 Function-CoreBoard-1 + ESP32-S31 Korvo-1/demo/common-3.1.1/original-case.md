##### 3.1.1 12h待机稳定性测试

**测试目的：** 验证设备未配网状态下，长时间待机，系统无异常

**前提条件：**

1、将设备上电
2、编译开启内存监控kasan和show_info测试工具的版本
开启show_info
LOW_RESOURCE_TEST=y
启用kasan

```
CONFIG_MM_KASAN=y
```

**步骤：**

1、设备静置12h，保存串口log

**预期结果：**

1、系统无报错、crash、重启等异常情况

---
