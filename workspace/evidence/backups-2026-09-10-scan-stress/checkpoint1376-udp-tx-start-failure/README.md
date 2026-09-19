# UDP TX1376 原始用例启动失败

5.1.25 原始命令包含12个argv，NSH有效上限11报too many arguments但继续执行；不能据此断言截断。ARP解析PC失败，约4.64秒返回shell，未完成300秒。保留FAIL，不计PASS。1379竞赛配置设置NSH_MAXARGUMENTS=16，等待构建和上板。原1354镜像同时执行1378指定SSID扫描100轮。
