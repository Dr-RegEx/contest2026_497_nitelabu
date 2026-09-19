# Camera / JPEG / RGB 静态接入复核（1714）

日期：2026-09-18。此记录只核对当前 openvela 源码的可接入边界，未把主机检查或源码存在误记成开发板通过。

## 已核实的现有链路

- `nuttx/drivers/video/Make.defs` 在 `CONFIG_DRIVERS_VIDEO=y`、`CONFIG_I2C=y` 且 `CONFIG_VIDEO_OV2640=y` 时会编译 `ov2640.c`。
- `nuttx/drivers/video/Kconfig` 为 OV2640 提供 I2C 地址 `0x21`、RGB565/YUV422 选择和 JPEG 输出/分辨率选择；`nuttx/include/nuttx/video/ov2640.h` 暴露 `ov2640_initialize()`。
- `nuttx/include/sys/videoio.h` 已定义 `V4L2_PIX_FMT_RGB24`（三字节 RGB888）和 `V4L2_PIX_FMT_RGB565`，也定义了 `V4L2_PIX_FMT_JPEG`。
- `nuttx/drivers/video/v4l2_cap.c` 当前只把 UYVY/YUYV/NV12/YUV420/RGB565/JPEG 等映射到 `imgdata`/`imgsensor`；RGB24/RGB888 没有对应的 `IMGDATA_PIX_FMT_*` 或 S31 capture lower-half 映射，不能据此宣称 RGB888 摄像头链路已完成。
- S31 架构头声明 `CONFIG_SOC_LCDCAM_CAM_SUPPORTED`、`CONFIG_SOC_JPEG_*_SUPPORTED`，而 `esp_hal_cam`、`esp_hal_jpeg` 和 `upper_hal_jpeg` 目录保存了 ESP-IDF 参考实现；`hal_esp32s31.cmake/.mk` 当前只加入 Camera include 路径，没有加入这些组件源文件，也没有 openvela 用户态 JPEG/Camera API。

## 当前结论

1. OV2640 是通用 NuttX I2C 传感器配置驱动，不是 S31 LCD-CAM/DVP capture 驱动；仅启用 `CONFIG_VIDEO_OV2640` 不能产生帧数据。
2. 直接把 ESP-IDF `upper_hal_jpeg` 源文件加入默认板级镜像不安全：它依赖 ESP-IDF FreeRTOS、DMA、heap caps、PM/PSRAM 和内部头文件，需要先建立受配置保护的 NuttX adapter 和 buffer ownership 规则。
3. 当前板级配置的 `CONFIG_VIDEO` 未启用，且工程没有 Function-CoreBoard-1 的摄像头型号、DVP 时钟/数据/同步引脚及显示/帧缓冲绑定。因此 Camera、JPEG 硬件编解码、RGB888/RGB565 板级链路仍然必须在有外围设备时实测。

## 可复核命令

```sh
rg -n 'CONFIG_VIDEO_OV2640|CONFIG_OV2640_JPEG|CONFIG_OV2640_RGB565' \
  openvela-dev/nuttx/drivers/video/Kconfig
rg -n 'V4L2_PIX_FMT_(RGB24|RGB565|JPEG)|IMGDATA_PIX_FMT_(RGB565|JPEG)' \
  openvela-dev/nuttx/include/sys/videoio.h \
  openvela-dev/nuttx/drivers/video/v4l2_cap.c
rg -n 'upper_hal_jpeg|esp_hal_jpeg|esp_hal_cam' \
  openvela-dev/nuttx/arch/risc-v/src/esp32s31/hal_esp32s31.cmake \
  openvela-dev/nuttx/arch/risc-v/src/esp32s31/hal_esp32s31.mk
```

这些检查用于防止把“有通用驱动/有 SoC 能力宏”误判为“本板已经能采集和编解码”。
