# Camera DMA 帧所有权修复

日期：2026-09-19。范围为清单第四部分 Camera；仅主机验证，未刷板、未采集真实图像。

锁定 SDK `esp_cam_ctlr_dvp_cam.c` 的 EOF ISR 先调用 `esp_cam_ctlr_dvp_start_trans` 获取下一帧缓冲，之后才调用上一帧完成回调。此前 S31 始终返回同一个缓冲，导致 HPWORK 复制上一帧时 DMA 已可覆盖它。

本次 `esp32s31_camera_dvp.c` 改为两个私有 DMA 缓冲及明确的 FREE/DMA/COPY 所有权。复制中的帧不会再次交给 DMA；两个缓冲均被占用时使用 SDK 自带备用缓冲，队列繁忙时丢弃当前帧而不覆盖待复制帧。只交付完整 RGB565 帧，短帧、超长帧不再作为成功图像交付。队列提交失败释放所有权。停止流时取消尚未执行的工作，运行中的工作持有其缓冲直至结束；不在 V4L2 持锁调用 stop 时同步等待工作线程。释放整个控制器仍按停止 DMA、同步取消工作、释放内存的顺序执行。

## 验证

- `python3 tools/tests/camera-dvp-ownership/run.py`：从实际生产源提取所有权结构、回调及停止函数，使用主机 OS/HAL 桩编译，ASan/UBSan 通过。覆盖 SDK 请求下一缓冲早于上一帧完成的顺序、复制延迟时的覆盖隔离、忙丢帧、短/超长帧拒绝、队列失败、停止时保留运行中所有权、100帧连续回调。
- `bash backups/2026-09-10-scan-stress/build-camera-dma-ownership.sh`：新输出目录完整编译链接退出0，源配置恢复。日志 `camera-dma-ownership-build.log`。
- 镜像：`openvela-dev/out/esp32s31-camera-dma-ownership/nuttx.bin`；镜像、配置、源文件和测试脚本哈希见 `camera-dma-ownership.sha256`。

## 边界

主机桩不验证真实 ISR/SMP 时序、DMA/cache 硬件一致性、V4L2用户缓冲生命周期、帧率或 OV2640 实物引脚。额外两个 DMA 缓冲加 SDK 备用缓冲需要按实际分辨率确认内存余量。仍需外接摄像头完成 SCCB、连续采集、应用停止/关闭并发及 RGB/JPEG 实测。本轮不计 Camera 或 xTS 实测通过。
