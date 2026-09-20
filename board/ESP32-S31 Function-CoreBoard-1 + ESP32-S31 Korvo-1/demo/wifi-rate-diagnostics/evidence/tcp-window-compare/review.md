# TCP 窗口对比本轮结果

使用已验证的 wifi-rx-observe 镜像和 Windows iperf2-select1656，在 BSSID 7c:39:85:2a:50:04 下比较 16k/64k/16k。三轮均未取得有效吞吐量汇总；首轮日志明确记录 ARP 超时及 Network is unreachable。不能据此判断窗口调优收益，也不能记为速率为零或通过。

此前该接入点 UDP 接收 30 秒为 5.18 Mbit/s、丢包 1.2%，不能据此推断 TCP 发送可达。AP6 目录名属于历史标签，实际本轮扫描显示该 BSSID 为 2412 MHz（信道 1）。
