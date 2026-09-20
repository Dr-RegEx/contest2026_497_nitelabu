# SDMMC 隔离 NuttX 候选边界（1752）

日期：2026-09-18。基于 1750 的能力审计，进一步核对 NuttX 接入点；未修改默认 profile、未刷板。

## 可复用入口

供应商 HAL 的 `upper_hal_sdmmc` 已提供足够底层事务接口：

- `legacy/include/driver/sdmmc_host.h`：`sdmmc_host_init()`、`sdmmc_host_init_slot()`、`sdmmc_host_do_transaction()`、时钟/总线宽度和 deinit；
- `legacy/include/driver/sdmmc_types.h`：`sdmmc_command_t`/响应和数据描述；
- `legacy/src/sdmmc_transaction.c`：将旧 API 转到 S31 `sd_host_slot_sdmmc_do_transaction()`；
- `src/sd_host_sdmmc.c`、`src/sd_trans_sdmmc.c`：S31 SDMMC host/DMA 事务实现；
- `components/esp_hal_sd/esp32s31/include/soc/sdmmc_pins.h`：slot 0 的固定 GPIO20–25 IOMUX。

NuttX 的协议层入口是 `nuttx/include/nuttx/sdio.h` 的 `struct sdio_dev_s`，块设备注册入口是 `nuttx/include/nuttx/mmcsd.h` 的 `mmcsd_slotinitialize(minor, sdio_dev)`。现有 `arch/xtensa/src/esp32s3/esp32s3_sdmmc.c` 可作为 vtable、DMA 缓存和中断同步的结构参考，但不能直接复用其 ESP32-S3 寄存器代码。

## 最小隔离候选

建议新建 `arch/risc-v/src/esp32s31/esp32s31_sdmmc.c`，只实现 slot 0 SD memory 所需的 `sdio_dev_s` 命令/读写/状态方法，并在 `boards/.../src/esp32s31_bringup.c` 增加受 `CONFIG_ESP32S31_SDMMC && CONFIG_MMCSD` 保护的：

1. `sdmmc_host_init()`；
2. `sdmmc_host_init_slot(SDMMC_HOST_SLOT_0, {CLK=24,CMD=25,D0..D3=20..23,width=4})`；
3. `sdio_initialize(0)` 等价的本板实现；
4. `mmcsd_slotinitialize(0, dev)` 注册 `/dev/mmcsd0`。

同时在 `arch/risc-v/src/esp32s31/Kconfig` 选择 `ARCH_HAVE_SDIO`，并把新源文件分别加入 `hal_esp32s31.cmake` 和 `hal_esp32s31.mk`。隔离 profile 才打开 `CONFIG_MMCSD=y`、`CONFIG_MMCSD_SDIO=y`、`CONFIG_ESP32S31_SDMMC=y`，默认镜像保持关闭。

## 当前阻断与验收顺序

这不是可以仅靠离线构建闭合的项目：`sdio_dev_s` 到 Espressif `sdmmc_command_t` 的完整协议适配尚不存在，且没有外接卡、供电/上拉和线序实物。最小下一步是先取得 SD 模块和 J2.26–31 线序，完成 slot 0 初始化与 CID/CSD 读取，再接块读写、FAT 挂载和受控掉电恢复；在此之前不得把 SDK 头文件或静态配置当作 SD 通过证据。
