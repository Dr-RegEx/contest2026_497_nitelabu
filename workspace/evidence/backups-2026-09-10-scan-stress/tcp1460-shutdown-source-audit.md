# TCP发送收尾路径核查：待运行时验证

用于冲刺5.1.23；1422原300秒已执行，但Windows接收端结尾Connection timed out。

源码证据：
- apps/external/iperf2/iperf2/src/Client.cpp:1473，FinishTrafficActions经tcp_shutdown调用shutdown(SHUT_WR)，再等待对端关闭。
- nuttx/net/tcp/tcp_shutdown.c:53，tcp_shutdown_eventhandler在TCP_POLL时立即清sndcb并请求TCP_TXCLOSE；未检查write_q/unacked_q或tx_unacked。
- nuttx/net/tcp/tcp_appsend.c:182，TCP_TXCLOSE将状态转FIN_WAIT_1，tx_unacked置1并发送FIN。
- tcp_send_buffered.c中write_q与未确认数据依赖sndcb推进/重传，因此存在未排空数据时关闭发送的风险。

这仅为源代码发现，尚未证明1422现场发生了该竞态。当前1457测试不改镜像、串口或连接。后续用带待发数据的有限TCP发送+SHUT_WR验证接收字节完整性和EOF，记录shutdown前队列/未确认字节及FIN序号；若证实再修复回调排空和错误收尾，然后原始300秒回归。不得通过延时、忽略接收端超时或缩短原负载伪造通过。

额外工具问题：Client.cpp:1655的recv返回值赋值括号将结果转为布尔值，导致rc<0分支不可达。不要据客户端无告警推断EOF；应使用接收端日志/协议证据。暂不修改原始iperf程序。
