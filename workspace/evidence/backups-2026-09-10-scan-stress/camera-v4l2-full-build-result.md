# Camera V4L2 独立镜像完整链接结果

日期：2026-09-19。范围：比赛清单用户自添加第5项 Camera；不表示实物通过。

既有 `xts-flat-camera` profile 已包含 V4L2 桥接，本次完整配置、编译和链接 1401 个构建步骤成功，补齐 1782 仅对象编译的证据。未修改默认配置，未刷板、未使用串口；source `.config` 和 `include/nuttx/config.h` 暂移后已恢复。

- 输出：`openvela-dev/out/esp32s31-xts-flat-camera-v4l2-verify/nuttx.bin`
- BIN SHA256：`891cf015f16c80e5e772d62107118a42b0ebd68ba6225993f3e9519ba79e447d`
- ELF SHA256：`944054d6e94ce8390f97d33b96547632a3c73e898d639e590d79dc0f672e8f47`
- 原始构建日志：[camera-v4l2-full-build.log](camera-v4l2-full-build.log)
- 复现构建：[build-camera-v4l2.sh](build-camera-v4l2.sh)。使用现有锁定 SDK、离线 HAL 和 Python 工具环境；与其它会改变 source 配置的构建互斥。

生成配置确认 `ESP32S31_CAMERA_OV2640`、`ESP32S31_CAMERA_DVP`、`ESP32S31_CAMERA_V4L2`、`VIDEO_STREAM`、`VIDEO_OV2640`、`EXAMPLES_CAMERA`、`SCHED_HPWORK` 均启用。最终 System.map 包含 `esp32s31_camera_v4l2_initialize`、DVP initialize/start/stop/set_target 和 `camera_main`，因此桥接实际进入链接镜像。

## 尚需真实硬件

需要外置 OV2640 DVP 模块及按 Function-CoreBoard 原理图核定的接线/供电；当前候选引脚来自 SDK 测试夹具，并非用户已确认接线：SCCB SCL=1、SDA=0；D0..D7=46..53；XCLK=55、PCLK=54、HREF=57、VSYNC=56。应先确认引脚在开发板上的可用性以及模块 RESET/PWDN、电平和时钟要求，不能直接照此候选表接线后宣称通过。

仍需上板证明 SCCB 识别、实际 `/dev/video0` 注册、640×480 RGB565 帧捕获及图像内容正确、连续启停恢复。该配置只提供固定 RGB565 Camera 路径，不代表 Camera JPEG 输出实测。没有传感器时保留“待实物测试”。
