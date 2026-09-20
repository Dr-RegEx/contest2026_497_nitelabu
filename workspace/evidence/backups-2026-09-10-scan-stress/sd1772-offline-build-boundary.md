# SDMMC 隔离构建边界（1772）

更新时间：2026-09-19

本轮只做离线配置和编译核验，没有刷写、串口占用或外接 SD 卡。

## 结果

- 新增隔离配置 `xts-flat-sdmmc`，未改变默认生产 profile。
- 修正 `ESP32S31_SDMMC` 的 Kconfig 门控：由直接依赖 `MMCSD_SDIO` 改为依赖 `MMCSD`，并选择 `ARCH_HAVE_SDIO`，使 `CONFIG_MMCSD_SDIO=y` 能在隔离配置中生效。
- 构建配置确认 `CONFIG_ESP32S31_SDMMC=y`、`CONFIG_ARCH_HAVE_SDIO=y`、`CONFIG_MMCSD_SDIO=y`。
- 构建在 `197/1333` 停止：锁定 IDF 的 `components/sdmmc/include/sd_protocol_types.h` 包含 `hal/sd_types.h`，当前 S31 CMake target 没有闭合该 HAL include/dependency。
- 1770 曾完成镜像链接，但 Kconfig 因 capability 门控丢弃 SDMMC 选项，不能作为 SDMMC 构建证据，故不计入完成。

## 结论

SDMMC 的 NuttX `sdio_dev_s` 桥接源已被正确选入配置，但锁定 HAL 的 CMake 依赖仍未闭合。当前没有卡识别、块读写、FAT 挂载、DMA/IRQ 或掉电恢复证据，不能标记通过；下一步应先补齐 `hal/sd_types.h` 所属 HAL include/dependency，再由实物完成验证。

证据：

- 配置：`openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/xts-flat-sdmmc/defconfig`
- 构建日志：`logs/sd1772-build.log`
- 镜像输出目录：`openvela-dev/out/esp32s31-xts-flat-sdmmc-1772`
