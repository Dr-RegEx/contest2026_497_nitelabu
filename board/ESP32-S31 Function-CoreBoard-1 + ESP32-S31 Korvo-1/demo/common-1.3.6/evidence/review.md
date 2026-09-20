# 1.3.6 GPIO实物测试：通过（本次功能范围）

2026-09-20用户确认已将S31 GPIO47与GPIO48通过一根杜邦线相连。输出GPIO48=/dev/gpio1，输入GPIO47=/dev/gpio0。USB UART重新附加到WSL。

| 测试 | 实测结果 | 原始日志 |
|---|---|---|
|输出0、回读0、回环输入0、文件读写、上升沿IRQ|4/4 PASS，IRQ count=1|kernel/level0.log|
|输出1、回读1、回环输入1、文件读写、上升沿IRQ|4/4 PASS，IRQ count=1|kernel/high-retry/level1.log|

命令：`cmocka_driver_gpio -i /dev/gpio0 -o /dev/gpio1 -p 0 -l -r 1`，高电平组将-p改为1。本地官方文档1.3.6示例为-a/-b，但当前原始源码解析器实际为-i/-o，按源码有效选项执行，不修改测试断言。原中断用例只断言poll非负，本次额外核对真实驱动IRQ count=1，未将超时当中断通过。

使用当前KERNEL独立profile demo-gpio-loopback，配置、kernel/AppFS及刷写读回凭据见kernel/receipt.json。移除了GPIO配置项不必要的BUILD_FLAT限制，驱动实现不变。加入cmocka所需MIT regex/setjmp支持；本次profile关闭iperf2/curl以控制AppFS分区占用。

限制及失败记录：历史886与当前FLAT镜像均在ABC早期启动停滞，未执行GPIO。KERNEL低电平组通过后，连续第二次启动测试仅回显未运行，主机20秒超时；重启后高电平组4/4通过。因此两个电平组属于同一配对镜像的分别启动测试，不宣称连续重复启动问题已修复。原失败日志均保留，不覆盖。其他中断触发模式未在本轮测试。

本项通过不代表Wi-Fi高负载资源耗尽问题已修复。板上当前保留GPIO KERNEL测试镜像。
