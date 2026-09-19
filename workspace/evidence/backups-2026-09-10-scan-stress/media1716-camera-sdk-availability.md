# Camera SDK availability audit (1716)

日期：2026-09-18。无需开发板即可确认的事实如下：

- `esp-hal-3rdparty/components/upper_hal_cam` 已包含 DVP controller 源码（`esp_cam_ctlr.c`、`dvp/src/esp_cam_ctlr_dvp_cam.c`、`dvp/src/esp_cam_ctlr_dvp_gdma.c`），并声明 `esp_hal_cam`、GPIO、DMA 依赖。
- 同目录有 `test_apps/dvp/sdkconfig.ci.esp32s31_ov2640`，打开 `CONFIG_CAMERA_OV2640=y`。
- `test_apps/dvp/main/test_dvp_ov2640_board.h` 已提供 S31 的 640×480、YUYV、20 MHz XCLK、8-bit DVP 以及 SCCB/数据/同步引脚映射；测试源会创建 DVP controller、分配 DMA 帧缓冲并检查传输回调。

这证明供应商 SDK 有可供移植的 S31 DVP/OV2640 实现和测试入口，但当前 NuttX `hal_esp32s31.cmake` 仅加入 `esp_hal_cam` 头文件路径，未把 `upper_hal_cam` 源码和 V4L2/NuttX 用户接口接入，且开发板没有摄像头实物。因此本记录只推进“已有 SDK 参考实现”审计，Camera 清单仍为待适配/待实物，不能记为通过。引脚是否适合 Function-CoreBoard-1 还必须依据实物原理图确认。
