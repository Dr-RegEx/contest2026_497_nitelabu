# 1782 Camera V4L2 bridge offline build

日期：2026-09-19
范围：只验证 Camera 用户自添加第 5 项的 NuttX `imgsensor_s`/`imgdata_s` 注册桥接和 DVP 帧回调边界；不刷机、不打开串口、不连接摄像头，不把静态编译结果记为实物或 xTS 通过。

## 本次实现

- 新增受 `CONFIG_ESP32S31_CAMERA_V4L2` 保护的板级 V4L2 桥接：注册 `/dev/video0`，提供 OV2640 的 RGB565 固定尺寸格式、帧率校验和 NuttX sensor/data ops。
- `imgdata_set_buf()` 将 V4L2 缓冲交给 DVP 桥接；DVP 继续使用 DMA-safe 内部帧，再把完整帧复制到 V4L2 缓冲。
- Espressif DVP 的 `on_trans_finished` 在 DMA ISR 中执行。ISR 仅保存接收长度并排入 HPWORK，复制、时间戳和 `imgdata_capture_t` 回调均在工作线程执行，避免在 ISR 中调用 V4L2/时间接口。
- `CONFIG_ESP32S31_CAMERA_V4L2` 只加入 `xts-flat-camera` 隔离 profile，默认生产配置仍不启用；DVP 选项显式依赖 `SCHED_HPWORK`，保证 ISR 延迟路径有工作线程。

## 交叉编译

使用已完成的 `out/esp32s31-xts-flat-camera-1774/compile_commands.json`、锁定 S31 GCC 和该 profile 的 `include/nuttx/config.h`，对新增/修改源执行 `-Werror` 交叉编译：

| 对象 | 结果 | SHA256 |
| --- | --- | --- |
| `arch/risc-v/src/esp32s31/esp32s31_camera_dvp.c` | 通过 | `e90a46b6c4f8090310b7dc43b0e768eceb945afdf35c78dacaff676040f60565` |
| `boards/.../src/esp32s31_camera.c`（V4L2 宏开启） | 通过 | `99cd44abf6bf67a615c55e09b48a6ac39ccf98d7a7ae9f3ceaf593f04c39107b` |
| `boards/.../src/esp32s31_camera_v4l2.c` | 通过 | `1b9e37325e9282bb085dca61bb5826eda6b9f88d59f97791fefd39b88ba59627` |

编译命令来源的隔离镜像已验证 `CONFIG_ESP32S31_CAMERA_DVP=y`、640x480、RGB565、`VIDEO_STREAM=y`；本次未改默认镜像，也未刷写目标板。

## 传统 Make 路径复核

补齐了不经过 CMake 的源文件入口：板级 `src/Make.defs` 在
`CONFIG_ESP32S31_CAMERA_V4L2=y` 时加入 `esp32s31_camera_v4l2.c`；
arch `Make.defs` 在 `CONFIG_ESP32S31_CAMERA_DVP=y` 时加入 DVP 控制器、GDMA、
`esp_hal_cam`、`color_hal` 等与 CMake 路径一致的源；`hal_esp32s31.mk` 增加
`upper_hal_cam` 的 include 路径。

在 `nuttx/arch/risc-v/src` 使用 `make -f esp32s31/Make.defs -pn` 并开启
`CONFIG_ESP32S31_CAMERA_DVP=y`，变量展开结果包含 `esp32s31_camera_dvp.c`、
五个 upper-HAL/DVP 源、`cam_hal.c`、`esp32s31/cam_periph.c` 和
`color_hal.c`；在板级 `src` 使用 `make -f Make.defs -pn` 并开启
`CONFIG_ESP32S31_CAMERA_V4L2=y`，`CSRCS` 包含
`esp32s31_camera.c esp32s31_camera_v4l2.c`。Make 解析通过；该直接解析环境
没有加载完整工程工具链，因此不把它宣称为整镜像构建。

本次 Make 路径文件哈希：

| 文件 | SHA256 |
| --- | --- |
| `boards/.../src/Make.defs` | `daede38841dd7a97d6b78b268b64b5bd7e0ffce2d2d0b5d4034e47cc074caf84` |
| `arch/.../esp32s31/Make.defs` | `b15d601c92a9561dc7dbd764848d2f5650c979913c6b7ccc0e509610336926c2` |
| `arch/.../hal_esp32s31.mk` | `0310d421ef6bd9696ccaf3c6fe05d61253830ef32642011e413ebee5191a3588` |

## 验收边界

这次结果证明了 V4L2 注册桥接、固定格式检查、DVP ISR 到 HPWORK 的帧通知路径可以通过 S31 交叉编译；它没有证明外部 OV2640 的 SCCB 探测、`/dev/video0` 实际创建、DVP 引脚时序、RGB565/JPEG 帧输出或 Camera xTS 通过。仍需连接真实传感器并在板上采集一帧后才能把清单状态改为通过。
