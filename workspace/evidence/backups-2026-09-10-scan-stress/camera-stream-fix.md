# Camera主机侧代码修复

2026-09-19，按用户要求只做主机侧适配，无刷板、串口或摄像头操作。

1. 修复V4L2持续采集：上层complete_capture通过set_buf轮换缓冲区，不会再次start_capture；原桥接set_buf传NULL回调会关闭后续帧通知。现保留已注册callback/arg并切换目标，仅底层成功后更新缓冲状态，拒绝空回调启动。
2. 修复DVP退出：先停止DMA回调源，再work_cancel_sync排空复制任务，之后释放控制器和帧缓冲。停止失败时保留资源并报错，避免ISR在取消后重新排队访问已释放内存。

原始对象检查首次受到source config搜索顺序影响，已改用强制包含Camera输出目录生成config.h，两个实际对象-Werror通过。随后全新输出目录完整构建成功，作为最终验证依据。此为代码与构建验证，尚无连续真实帧、DMA波形或摄像头实测。

- [完整构建日志](camera-stream-fix-build.log)
- [固件、配置、源码哈希](camera-stream-fix.sha256)
- [复现脚本](build-camera-stream-fix.sh)
- 固件：`openvela-dev/out/esp32s31-camera-stream-fix/nuttx.bin`

仍需实物验证SCCB、真实帧、DMA缓冲所有权与并发启停；不把构建成功标成Camera通过。
