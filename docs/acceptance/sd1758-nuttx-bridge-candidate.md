# SDMMC 到 NuttX `sdio_dev_s` 桥接候选（1758）

日期：2026-09-19。该记录只覆盖 SD 卡用户自添加第 11 项的离线软件边界，不代表外接卡、协议或文件系统通过。

## 已落地的隔离实现

- 新增 `arch/risc-v/src/esp32s31/esp32s31_sdmmc.c` 与公开头文件，填充 NuttX `sdio_dev_s` 方法表。
- `sendcmd` 将 NuttX 的命令索引、响应类型、读写/数据传输标志映射为锁定 ESP-IDF 6.1 的 `sdmmc_command_t`；`recv_r1`/`recv_r2`/`recv_r3..r7` 从同一响应缓冲区返回结果。
- `blocksetup`、`recvsetup`、`sendsetup` 将块长、块数和 DMA 数据缓冲区映射到 `sdmmc_command_t`；时钟、总线宽度和 Host/slot 初始化入口也已固定到 slot 0。
- S31 slot 0 使用 `SDMMC_SLOT_CONFIG_DEFAULT()` 后强制 4-bit，固定 `CLK=GPIO24`、`CMD=GPIO25`、`D0..D3=GPIO20..23`，不假设卡检测/写保护脚。
- 新增 `CONFIG_ESP32S31_SDMMC`，依赖 `MMCSD && MMCSD_SDIO && !SMP`，默认关闭；板级 bring-up 仅在显式开启时尝试注册 minor 0。
- HAL 调用使用弱符号探测。未将 HAL 源依赖、DMA/IRQ 完成链和外接卡初始化强行并入默认镜像；缺失依赖时返回 `-ENOSYS`，不会伪造设备可用。

## 离线验证

使用现有 `esp32s31-xts-flat-usb-adb/compile_commands.json` 的 S31 交叉编译器，加上锁定 HAL 的 `upper_hal_sdmmc`、`upper_hal_sd_intf`、`esp_hal_sd` 和 ESP-IDF `components/sdmmc/include` 头文件路径：

```text
esp32s31_sdmmc.c: -fsyntax-only -> PASS
esp32s31_bringup.c (CONFIG_ESP32S31_SDMMC/MMCSD/MMCSD_SDIO=1): -fsyntax-only -> PASS
```

这两个检查没有生成或刷写镜像，没有占用 UART，也没有调用 SDMMC Host。默认 `production`、现有 xTS 和竞赛 profile 仍关闭 `CONFIG_MMCSD`/`CONFIG_ESP32S31_SDMMC`。

## 尚未闭合的边界

锁定 HAL 的 `upper_hal_sdmmc` 实现还依赖其 controller、OS/DMA、事件队列和中断源，当前桥接候选没有注册这些依赖；因此没有 `/dev/mmcsd0`、CID/CSD、容量、单/多块读写、FAT 挂载、掉电恢复或实物日志。该候选不增加 xTS 或比赛清单的通过数，下一步需外接带 3.3 V/上拉的 SD 模块后再做板上验证。
