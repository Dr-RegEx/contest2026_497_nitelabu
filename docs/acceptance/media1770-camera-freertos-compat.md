# Camera DVP FreeRTOS 兼容层核验（1770）

日期：2026-09-19  
范围：只做锁定 ESP-IDF upper HAL 的离线交叉编译；不刷板、不打开串口、不连接摄像头。

## 兼容层

新增 `openvela-dev/nuttx/arch/risc-v/src/esp32s31/include/freertos/FreeRTOS.h`，仅由 S31 DVP 隔离源的已有芯片 include 路径发现。它没有伪造空临界区，而是将 IDF 的 `portMUX_TYPE` 映射为 NuttX `rspinlock_t`，并为每个 CPU 保存最外层 `irqstate_t`：

- `portENTER_CRITICAL` 调用 `rspin_lock_irqsave`，首次持有时保存中断状态；
- 递归进入只增加 NuttX rspinlock 计数；
- `portEXIT_CRITICAL` 调用 `rspin_unlock_irqrestore`，只在最后一层恢复外层状态。

因此保留了 IDF 临界区所需的递归、跨 CPU 互斥和中断恢复语义。该头文件不提供 FreeRTOS 调度、任务、队列或延时 API，不能被误认为完整 FreeRTOS 移植；当前 DVP HAL 只引用上述 critical/mux 表面。

## 源码级结果

使用隔离构建 `out/esp32s31-xts-flat-camera-1767/compile_commands.json`，对以下锁定 HAL 源重新编译到 `/tmp`：

```text
dvp_share_ctrl.c                 RC=0
esp_cam_ctlr_dvp_cam.c           RC=0
esp_cam_ctlr_dvp_gdma.c          RC=0
```

追加 `-Werror=implicit-function-declaration -Werror=incompatible-pointer-types` 后三者仍全部 `RC=0`。这证明 `freertos/FreeRTOS.h` 缺失边界已闭合，且没有以空宏绕过编译。

## 当前边界与后续镜像核验

按隔离脚本流程暂存旧的 `nuttx/.config` 和 `nuttx/include/nuttx/config.h` 后，完整构建重新执行。DVP 相关对象均已实际编译成功：

```text
esp32s31_camera_dvp.c       4608 bytes
dvp_share_ctrl.c            2064 bytes
esp_cam_ctlr.c              3940 bytes
esp_cam_ctlr_dvp_cam.c     17944 bytes
esp_cam_ctlr_dvp_gdma.c     4384 bytes
```

第一次完整构建在 `216/1383` 处停止于与 Camera 无关的既有 RISC-V 汇编工具链边界：

```text
/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/arch/risc-v/src/common/riscv_fpu.S:74:
Error: unrecognized opcode `fence.i', extension `zifencei' required
```

此前未暂存旧生成头的手工重配置曾误报 `CONFIG_SPIRAM_SPEED` 缺失；按隔离脚本流程重跑后该项不再出现。第一次停止由构建环境未将锁定工具链的 `bin` 目录加入 `PATH` 引起，未修改默认生产 profile。补齐工具链路径后，后续证据 [media1774](media1774-camera-dvp-linked-build.md) 已完成同一 `xts-flat-camera` profile 的 `1400/1400` 编译、链接并生成 `nuttx.bin`（611628 字节）。另一个全新输出目录在 `resetconfig` 阶段仍触发已有 `ARCH_HAVE_SDIO -> ESP32S31_SDMMC -> MMCSD_SDIO` Kconfig dependency loop。FreeRTOS 兼容头和 Camera/DVP 离线镜像边界现已闭合；Camera 仍不能计为最终实物或 xTS 通过，尚无 `/dev/video0`、SCCB 探测、帧输出或摄像头接线证据。
