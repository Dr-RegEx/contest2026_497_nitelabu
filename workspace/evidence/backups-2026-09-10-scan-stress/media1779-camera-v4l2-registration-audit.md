# Camera V4L2 注册边界审计（1779）

日期：2026-09-19

本轮只检查用户自添加第 5 项 Camera 的 `/dev/video0` 注册路径，没有刷写、串口操作或摄像头实物。

## 当前源码路径

- `boards/.../esp32s31_camera.c` 能调用 `ov2640_initialize(i2c)` 完成 SCCB 复位、芯片 ID 读取和寄存器初始化。
- `arch/risc-v/src/esp32s31/esp32s31_camera_dvp.c` 已完成锁定 ESP-IDF DVP 控制器的隔离链接，提供控制器 enable/start/stop 和 DMA 帧缓冲回调。
- NuttX 的 `capture_register()` 要求 `imgdata_s` 和 `imgsensor_s` 两组完整 ops；注册后才会由 `video_register()` 建立 `/dev/video*`。

## 阻断点

锁定的 `drivers/video/ov2640.c` 只提供 `ov2640_initialize()`，没有 NuttX `imgsensor_s` 对象、格式/帧间隔能力表、start/stop capture 和 control ops。当前 DVP 回调也只记录 `received_size`，没有把采集帧交给 `imgdata_capture_t`，没有实现 `imgdata_s` 的 `set_buf`/`start_capture`/`stop_capture` 生命周期。

因此直接在板级源中调用 `capture_register("/dev/video0", ...)` 会需要空壳 sensor/data 回调，既不能证明 SCCB 设备存在，也不能产生真实帧；本轮没有添加这种伪注册。

## 结论

Camera 1774 隔离镜像链接证据保持有效，但 `/dev/video0`、帧缓冲交付、RGB565/JPEG 实时链路仍未闭合。下一步必须补齐真实 OV2640 `imgsensor` 适配和 DVP 到 `imgdata` 的帧回调，并在接入摄像头后完成 SCCB、注册和帧采集实测；本轮不增加 xTS 或用户项目通过数。
