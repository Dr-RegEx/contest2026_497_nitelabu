# Camera DVP 隔离镜像链接核验（1774）

日期：2026-09-19
范围：仅做隔离 profile 的离线配置、编译和链接；未刷写、未打开串口、未连接摄像头。

## 结果

- profile：`esp32s31-core-function-board:xts-flat-camera`
- 构建：`1400/1400`，RC=0
- 输出：`openvela-dev/out/esp32s31-xts-flat-camera-1774/nuttx.bin`
- 镜像大小：611628 字节
- SHA256：`178d3a38d2a7cadda08af137282d1df923531e2e579054521c4f249ad3697a93`
- 配置确认：`CONFIG_ESP32S31_CAMERA_DVP=y`、`CONFIG_ESPRESSIF_SPIRAM_SPEED_80M=y`、`CONFIG_EXAMPLES_CAMERA=y`

## 链接符号

`System.map` 已包含：`esp32s31_camera_dvp_initialize`、`esp32s31_camera_dvp_uninitialize`、`esp_cam_new_dvp_ctlr`、`color_hal_pixel_format_fourcc_get_bit_depth`、`camera_main`、`jpegtest_main`。

本轮纳入锁定 HAL 的 `components/hal/color_hal.c`，补齐上一轮的最终链接缺口。DVP 的 NuttX FreeRTOS critical/mux 兼容头及严格对象编译结果见 [media1770](media1770-camera-freertos-compat.md)。

## 验收边界

镜像链接成功只证明驱动入口和 HAL 依赖已闭合，不证明板上存在摄像头、SCCB 探测、`/dev/video0` 注册、帧采集或 RGB/JPEG 实时链路。Camera 仍需接线和实物采集后才能标记通过；不增加 xTS 通过数。
