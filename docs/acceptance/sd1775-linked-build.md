# SDMMC 隔离镜像链接核验（1775）

日期：2026-09-19
范围：仅做隔离 profile 的离线配置、编译和链接；未刷写、未打开串口、未连接 SD 卡。

## 结果

- profile：`esp32s31-core-function-board:xts-flat-sdmmc`
- 构建：`1333/1333`，RC=0
- 输出：`openvela-dev/out/esp32s31-xts-flat-sdmmc-1775/nuttx.bin`
- 镜像大小：349596 字节
- SHA256：`31a2cd8b0f8c42aaa0fabd57d2f7953226d2224225eb651cb5120008d6493432`
- 配置确认：`CONFIG_ESP32S31_SDMMC=y`、`CONFIG_ARCH_HAVE_SDIO=y`、`CONFIG_MMCSD=y`、`CONFIG_MMCSD_SDIO=y`

## 链接符号

`System.map` 已包含 `esp32s31_sdmmc_initialize`、`esp32s31_sdmmc_register`、`sdmmc_host_prepare` 以及 NuttX `mmcsd_slotinitialize`/读写路径。补入 `esp_hal_sd/include` 后，上一轮的 `hal/sd_types.h` include 缺口已闭合。

## 验收边界

该镜像仍使用桥接层的弱 `sdmmc_host_*` 依赖；链接成功不证明 S31 SDMMC controller、DMA/IRQ、卡识别、块读写、FAT 挂载或掉电恢复已经可用。SDMMC 仍需外接卡和板上实测后才能标记通过；不增加 xTS 通过数。
