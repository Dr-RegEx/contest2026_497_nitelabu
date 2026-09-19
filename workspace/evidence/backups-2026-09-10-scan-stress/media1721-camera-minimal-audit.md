# Camera 摄像头采集最小核验（1721）

日期：2026-09-18。本文只记录源码、配置和供应商 SDK 的可复核核验，不把静态审计当作开发板采集通过。

## 结论

比赛清单“用户自添加第5项 Camera 摄像头采集”仍为**待适配、未实物验证**。当前 Function-CoreBoard-1 没有摄像头实物，当前 1720 镜像也没有启用视频设备，因此本轮不能形成真实帧采集证据。

## 已核实内容

1. 锁定的 S31 SDK 有 DVP/OV2640 测试入口：
   `s31-reference/deps/esp-hal-3rdparty/components/upper_hal_cam/test_apps/dvp/`。
   `test_dvp_ov2640_board.h` 为 S31 定义 640x480、8-bit YUYV、20 MHz XCLK，期望约 6 fps；引脚为 SCCB SCL/SDA GPIO1/0，D0-D7 GPIO46-53，PCLK GPIO54，XCLK GPIO55，VSYNC GPIO56，DE GPIO57，RESET GPIO60，PWDN GPIO61。
2. 测试源 `test_dvp_ov2640.c` 会创建 DVP controller、分配 DMA 帧缓冲、初始化 OV2640、启动约 1 秒采集，并检查新帧/完成回调计数。它依赖 ESP-IDF `esp_cam_ctlr`、FreeRTOS、heap caps 和 `sensor_init`，不能直接作为 NuttX 用户态程序。
3. S31 架构头已声明 `CONFIG_SOC_LCDCAM_CAM_SUPPORTED` 和 `CONFIG_SOC_JPEG_CODEC_SUPPORTED`；这证明 SoC 能力存在，但不等于板级驱动已经接通。
4. `hal_esp32s31.cmake`/`.mk` 当前只加入 `esp_hal_cam` 头文件路径，没有加入 `upper_hal_cam` 源文件，也没有 V4L2 capture lower-half、Function-CoreBoard-1 的摄像头引脚绑定或 `/dev/video*` 注册。
5. 当前板上配置 `openvela-dev/out/esp32s31-spawn-kernel1720/.config` 明确为 `# CONFIG_DRIVERS_VIDEO is not set`、`# CONFIG_VIDEO is not set`，因此当前镜像没有视频采集入口。

## 引脚和实物边界

供应商参考 DVP 引脚与当前板级测试资源存在冲突：GPIO45/46 被 BMI160 I2C 固定使用，GPIO47/48 被 GPIO 回环/ADC/PWM 测试使用，GPIO50/51 是板级 I2C0，GPIO61 是 BOOT 按键输入。即使补充 OV2640 模块，也必须先依据 Function-CoreBoard-1 原理图确认可用复用引脚并建立独立配置，不能直接套用 SDK 参考引脚。

## 最小接入顺序

确认摄像头型号和实际可用 DVP 引脚后，再增加受配置保护的 S31 capture lower-half，接入 `upper_hal_cam` 的 DMA/回调和 OV2640 SCCB 初始化，注册 V4L2 `/dev/video0`；随后用 640x480 YUYV 帧计数、缓冲长度和帧内容做板上证据。没有模块和引脚确认前，Camera 项目不应改为通过。

## 核验命令与结果摘要

```text
rg CONFIG_(DRIVERS_VIDEO|VIDEO) openvela-dev/out/esp32s31-spawn-kernel1720/.config
  CONFIG_DRIVERS_VIDEO: not set
  CONFIG_VIDEO: not set

rg upper_hal_cam|esp_cam_ctlr openvela-dev/nuttx/arch/risc-v/src/esp32s31/hal_esp32s31.{cmake,mk}
  only esp_hal_cam include paths; no upper_hal_cam source registration
```

本记录没有执行板上 Camera 测试，也没有产生帧文件；其用途是为清单保留当前可验证边界和下一步接入条件。
