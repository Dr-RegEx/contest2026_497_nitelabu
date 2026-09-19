# Camera DVP 隔离构建边界（1767）

日期：2026-09-19  
范围：只做锁定 ESP-IDF HAL、S31 DVP 源和隔离 `xts-flat-camera` 镜像的离线交叉编译；不刷板、不打开串口、不连接摄像头。

## 已接入的候选路径

- `nuttx/arch/risc-v/src/esp32s31/esp32s31_camera_dvp.c`：调用锁定 SDK 的 `esp_cam_new_dvp_ctlr`、DMA buffer、事件回调、enable/start/stop/disable/delete 生命周期。
- `nuttx/arch/risc-v/src/esp32s31/include/esp32s31_camera_dvp.h`：板级初始化、启动、停止和释放接口。
- `nuttx/arch/risc-v/src/esp32s31/CMakeLists.txt`：仅在 `CONFIG_ESP32S31_CAMERA_DVP` 下加入 upper HAL camera 源。
- `xts-flat-camera`：保持隔离 profile，默认生产配置未启用；引脚和 640x480 RGB565 参数来自锁定 S31 DVP OV2640 参考测试。

## 构建结果

使用隔离输出目录 `out/esp32s31-xts-flat-camera-1767` 配置并构建，目标在 `1399` 个任务中的第 `188` 步失败。失败源文件为锁定 HAL 的 `upper_hal_cam/dvp_share_ctrl.c`，错误为：

```text
fatal error: freertos/FreeRTOS.h: No such file or directory
```

因此当前 openvela/NuttX 构建树缺少该 upper HAL 所需的 FreeRTOS 兼容层，不能把这次候选源接入称为可链接的 DVP 适配。没有添加伪造 FreeRTOS 头文件，也没有把失败记作 Camera 通过。

## 结论

Camera 当前比 1764 的接口审计前进一步：DVP 控制器生命周期候选已写入隔离 profile，但 HAL/RTOS 适配仍未闭合，尚无 `video_register`、`/dev/video0`、帧输出或实物采集证据。下一步需要为锁定 upper HAL 提供经过验证的 NuttX 兼容层，随后才能进行真实引脚和摄像头测试。

