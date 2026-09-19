# Camera DVP/V4L2 接口边界审计（1764）

## 目的

本记录只核对锁定的 ESP-IDF v6.1 S31 Camera/DVP API 与当前 NuttX 视频接口
之间的接入边界。当前没有摄像头外围设备，未刷写镜像、未打开串口、未执行
板级采集；因此本记录不构成 Camera 通过证据。

## 已确认的供应商 DVP 接口

参考工程 `s31-reference/tmp/esp-idf-clean` 提供
`components/esp_driver_cam/dvp/include/esp_cam_ctlr_dvp.h` 和
`components/esp_driver_cam/include/esp_cam_ctlr.h`。DVP 接入至少需要：

- `esp_cam_new_dvp_ctlr()` 创建控制器；
- `esp_cam_ctlr_register_event_callbacks()` 注册取新缓冲和完成回调；
- `esp_cam_ctlr_alloc_buffer()` 分配 DMA 帧缓冲；
- `esp_cam_ctlr_enable()`、`esp_cam_ctlr_start()` 启动；
- `esp_cam_ctlr_receive()` 或完成回调回收帧；
- `esp_cam_ctlr_stop()`、`esp_cam_ctlr_disable()`、`esp_cam_ctlr_del()` 释放。

S31 DVP 配置还必须提供 8 位数据线、PCLK、VSYNC、DE、XCLK 以及有效的
SCCB 引脚。供应商 `examples/peripherals/camera/dvp_dsi/main/dvp_dsi_main.c`
同时依赖 `sensor_init`（传感器句柄和格式枚举），不是只调用
`ov2640_initialize()` 就能得到帧。

## 当前 NuttX 接口缺口

`openvela-dev/nuttx/drivers/video/v4l2_cap.c` 的捕获层要求同时拥有：

- `struct imgsensor_s`：格式协商、`start_capture`/`stop_capture` 和控制项；
- `struct imgdata_s`：DMA 缓冲设置、格式校验、启动采集和完成回调；
- `video_register("/dev/video0", ...)` 注册 V4L2 节点。

当前板级 `esp32s31_camera.c` 仅调用 `ov2640_initialize(i2c)`。该函数返回
寄存器探测/配置状态，不返回 `imgsensor_s`，也不创建 `imgdata_s`、DVP
控制器、DMA 缓冲或 `/dev/video0`。因此即使 SCCB 探测源能交叉编译，仍不能
宣称 Camera capture、JPEG 摄像头链路或 RGB 帧输出完成。

## 引脚与资源风险

供应商示例的 DVP 引脚不能直接套用到 Function-CoreBoard-1。历史审计
`media1721-camera-minimal-audit.md` 已记录 GPIO45/46、47/48、50/51、61
分别被 BMI160、GPIO/ADC/PWM、板载 I2C0、BOOT 按键占用或保留；实际传感器
型号和可复用引脚必须先由原理图/实物确认，再建立独立 profile。

## 结论

当前最小安全边界是保留受配置保护的 OV2640 SCCB 探测和 `xts-flat-camera`
编译候选（证据 `media1759`、`media1761`、`media1726`），不添加伪造的
capture lower-half。后续只有在确认传感器、DVP 引脚和 HAL/OS/DMA/IRQ 依赖
后，才能实现 `imgsensor`/`imgdata` 适配、注册 `/dev/video0`，再进行实物
帧计数、缓冲长度、像素格式和 JPEG 输出验收。

## 离线核对结果

```text
OV2640_INITIALIZE_RETURNS_SENSOR_OBJECT=0
BOARD_CAMERA_SOURCE_HAS_VIDEO_REGISTER=0
BOARD_CAMERA_SOURCE_HAS_DVP_CONTROLLER=0
NUTTX_V4L2_REQUIRES_IMGSENSOR_AND_IMGDATA=1
HARDWARE_CAPTURE_EVIDENCE=0
```

实际路径和符号静态核对（2026-09-19）：

```text
IDF_DVP_HEADER_EXISTS=1
IDF_CTLR_HEADER_EXISTS=1
NUTTX_VIDEO_HEADER_EXISTS=1
NUTTX_IMGSENSOR_HEADER_EXISTS=1
NUTTX_IMGDATA_HEADER_EXISTS=1
esp_cam_new_dvp_ctlr_DECLARED=1
esp_cam_ctlr_register_event_callbacks_DECLARED=1
esp_cam_ctlr_receive_DECLARED=1
esp_cam_ctlr_alloc_buffer_DECLARED=1
BOARD_CAMERA_OV2640_INITIALIZE=1
BOARD_CAMERA_VIDEO_REGISTER=0
BOARD_CAMERA_ESP_CAM_NEW_DVP_CTLR=0
BOARD_CAMERA_IMGDATA_REGISTER=0
```
