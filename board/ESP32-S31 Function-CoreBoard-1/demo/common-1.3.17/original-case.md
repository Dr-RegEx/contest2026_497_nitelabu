##### 1.3.17 Crypto功能测试

**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.3 驱动BSP

**前提条件：**

1、打开如下配置

```
CONFIG_TESTING_CRYPTO=y
CONFIG_TESTING_CRYPTO_3DES_CBC=y
CONFIG_TESTING_CRYPTO_AES_CBC=y
CONFIG_TESTING_CRYPTO_AES_CTR=y
CONFIG_TESTING_CRYPTO_3DES_XTS=y
CONFIG_TESTING_CRYPTO_HMAC=y
CONFIG_TESTING_CRYPTO_HASH=y
CONFIG_TESTING_CRYPTO_CRC32=y
CONFIG_TESTING_CRYPTO_ECDSA=y
```

1）若厂商硬件支持des3cbc算法，则需打开配置
2）若厂商硬件支持aescbc算法，则需打开配置
3）若厂商硬件支持aesctr算法，则需打开配置
4）若厂商硬件支持aesxts算法，则需打开配置
5）若厂商硬件支持hmac算法，则需打开配置
6）若厂商硬件支持hash算法，则需打开配置
7）若厂商硬件支持crc32算法，则需打开配置
8）若厂商硬件支持ecdsa算法，则需打开配置
2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口
注：Vela Crypto框架提供了des3cbc、aescbc、aesctr、aesxts、hmac, hash, crc32, ecdsa这六个测试应用，厂商根据实际硬件实现的算法连接测试

**步骤：**

1、确认厂商硬件实现的算法，并在nsh中输入对应算法名称，等待执行结果
举例：若厂商硬件实现了des3cbc算法，即在nsh中输入 des3cbc即可

**预期结果：**

测试程序正常运行结束，并显示结果ok

---
