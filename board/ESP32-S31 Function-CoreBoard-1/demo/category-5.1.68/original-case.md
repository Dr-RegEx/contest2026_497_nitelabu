##### 5.1.68 KVDB稳定性vela_kvdb_stability_test02测试

**测试目的：** 二、品类自测用例 > 5、性能测试 > 5.1 系统应用 > KVDB

**前提条件：**

1.打开如下配置编译烧录

```
CONFIG_TESTS_TESTCASES=y
CONFIG_KVDB_TEST=y
CONFIG_KVDB_TEST_STABILITY=y
```

**步骤：**

1.进入nsh
2.删除/data目录下的文件persist.db，删除后重启设备
3.执行命令 vela_kvdb_stability_test02 10
（注意参数10是执行轮次，没有上线，执行10次约12分钟）

**预期结果：**

执行成功打印 TEST PASSED!
失败打印 TEST FAILED!

---
