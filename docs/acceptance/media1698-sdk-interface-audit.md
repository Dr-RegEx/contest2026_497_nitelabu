# JPEG / Camera / RGB 接口审计（1698）

日期：2026-09-18。范围：锁定参考工程 `s31-reference/tmp/esp-idf-clean` 与当前 openvela 板级工程。

## 已确认的芯片 SDK 能力

- JPEG 编码接口：`components/esp_driver_jpeg/include/driver/jpeg_encode.h` 提供 `jpeg_new_encoder_engine()`、`jpeg_encoder_process()`、`jpeg_del_encoder_engine()`。
- JPEG 解码接口：`components/esp_driver_jpeg/include/driver/jpeg_decode.h` 提供 `jpeg_new_decoder_engine()`、`jpeg_decoder_process()`、`jpeg_del_decoder_engine()`，并注明输出尺寸按 JPEG 协议按 16 字节对齐。
- DVP 摄像头：`components/esp_driver_cam/dvp/include/esp_cam_ctlr_dvp.h` 提供 DVP 控制器，含输入/输出像素格式、8/16/24 位数据宽度、JPEG 输入标志、字节交换和 DMA burst 配置。
- 摄像头通用控制：`components/esp_driver_cam/include/esp_cam_ctlr.h` 提供 enable/start/receive、帧缓冲、DMA 缓冲分配和格式转换接口。
- RGB 显示：`components/esp_lcd/rgb/include/esp_lcd_panel_rgb.h` 提供 RGB panel 时序；参考 HAL 明确包含 RGB565、RGB888 数据类型和转换路径。

## 当前 openvela 工程结论

1. 当前板级 NuttX 配置未启用 `CONFIG_LIBJPEG`，且 `apps/graphics/libjpeg/libjpeg` 源目录为空；直接启用会触发外部 libjpeg 下载，不能在本轮离线工程中宣称已接入。
2. 当前 openvela 树已保存 `esp_hal_jpeg`、`upper_hal_jpeg`、`esp_hal_cam` 及 S31 LCD-CAM/JPEG 寄存器定义；但 `hal_esp32s31.mk/cmake` 目前只为 Camera HAL 添加了 include 路径，没有把 JPEG/Camera 上层源文件接入构建，也没有对应 openvela 用户态 API。源码存在不等于目标镜像可调用。
3. 当前 Function-CoreBoard-1 没有摄像头实物在场，因此 Camera、JPEG 摄像头链路及 RGB 数据通路仍不能做板级验收。
4. RGB888/RGB565 的参考 HAL 能力已确认，但尚无当前板上的帧缓冲、显示面板或摄像头数据链路证据，仍保持待适配状态。

## 后续最小接入顺序

1. 先确定摄像头接口和引脚（DVP 或 CSI）及板上是否存在 RGB 接收/显示端。
2. 在 openvela 中增加受配置保护的适配层，映射 JPEG、Camera buffer/receive 和 RGB format conversion；保留 IDF 错误码到 errno 的转换。
3. 用固定静态 RGB565/RGB888 测试帧做格式和字节序自测，再接摄像头实物。
4. 只有完成真实输入、输出长度/帧内容核验后，才把清单中的用户新增第3～7项改为通过。

结论：本记录是接口和缺口审计，不是 JPEG、Camera 或 RGB 的通过证据。

## 1711 离线软件烟测补充

`media1711-jpeg-rgb-host-smoke.c` 使用仓库内 vendored libjpeg-turbo 源码在主机完成了 2×2 RGB888 编码、内存 JPEG 解码，以及 RGB888/RGB565 5/6/5 打包解包检查。运行结果见 `media1711-jpeg-rgb-host-smoke.md`。该结果已将清单中的 JPEG 编码、JPEG 解码、RGB888 和 RGB565 状态提升为“软件烟测完成、待板级链路验证”，没有改变 Camera 的待适配/待实物状态，也没有宣称任何板级通过。

## openvela 侧可复用但尚未形成板级证据的代码

- `nuttx/include/nuttx/video/rgbcolors.h:74-88` 已有 `RGB24TO16` 与 `RGB16TO24` 宏，可作为 RGB565/RGB888 软件转换基础；这只证明通用库代码存在，不能证明本板 Camera 或显示链路可用。
- `apps/examples/camera` 使用 NuttX V4L2/视频驱动并支持 RGB565 输出，但当前 1676 网络镜像 `.config` 中 `CONFIG_VIDEO` 未启用，也没有 ESP32-S31 摄像头传感器和引脚配置。

## S31 硬件能力边界

`nuttx/arch/risc-v/src/esp32s31/include/sdkconfig.h` 已声明 `CONFIG_SOC_LCDCAM_CAM_SUPPORTED`、`CONFIG_SOC_LCDCAM_RGB_LCD_SUPPORTED`、`CONFIG_SOC_JPEG_CODEC_SUPPORTED`、`CONFIG_SOC_JPEG_DECODE_SUPPORTED` 和 `CONFIG_SOC_JPEG_ENCODE_SUPPORTED`。这确认芯片能力已反映到架构头文件，但当前工程仍缺少把这些能力暴露给 openvela 应用的驱动层；因此不能把 SoC capability 宏当成适配完成。


## 已有移植资产的精确位置

- JPEG 上层实现：`nuttx/arch/risc-v/src/esp32s31/esp-hal-3rdparty/components/upper_hal_jpeg/`，含 `jpeg_encode.c`、`jpeg_decode.c` 和 `driver/jpeg_{encode,decode}.h`。
- JPEG 底层 HAL：`.../components/esp_hal_jpeg/`，含 S31 `jpeg_periph.c`、`jpeg_ll.h`。
- Camera 底层 HAL：`.../components/esp_hal_cam/`，含 S31 `cam_periph.c`、`cam_ll.h`。
- 当前构建检索结果：`hal_esp32s31.mk/cmake` 未引用 `upper_hal_jpeg` 或 `esp_hal_jpeg` 源文件；因此尚未形成可执行镜像证据。
