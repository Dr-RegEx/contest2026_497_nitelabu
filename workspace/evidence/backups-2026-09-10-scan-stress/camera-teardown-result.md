# Camera HAL 释放失败路径修复

2026-09-19，仅主机工作。控制器disable/delete失败时原代码仍释放DMA帧并清空句柄，可能使尚未释放的控制器保留悬空地址。现在失败时保留控制器和帧，可再次尝试；disable成功时记录enabled=false，不重复disable。

`tools/tests/camera-dvp-ownership/run.py`使用生产释放函数，注入stop/disable/delete逐阶段失败，再验证重试成功只释放两帧一次。ASan/UBSan回归通过，日志 `camera-teardown-host-test.log`。沿上轮Camera冻结配置执行本对象严格交叉编译，`-Werror`通过（`camera-teardown-compile-fixed.log`）；首次配置强制包含顺序错误导致的失败日志保留，没有伪造配置宏。

本轮未完整重建/刷板；上一轮 `esp32s31-camera-dma-ownership/nuttx.bin`不含本次释放修复。其原始驱动另存 `camera-dma-ownership-source.c`，已按原哈希核对一致；新源快照 `camera-teardown-source.c`。硬件连续采集及V4L2用户缓冲并发生命周期仍需实测/后续完善，不计新增通过。
