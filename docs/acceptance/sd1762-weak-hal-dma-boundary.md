# SDMMC controller/HAL 弱依赖与 DMA 边界核验（1762）

日期：2026-09-19。此记录只覆盖离线源级集成边界，不代表外接 SD 卡、协议、块设备、FAT 或掉电恢复通过。

## 本次变更

- `esp32s31_sdmmc.c` 为锁定 ESP-IDF 6.1 `SDMMC_HOST_DEFAULT()` 使用的完整 legacy Host API 集合增加弱符号声明：初始化、slot、总线宽度、时钟、事务、延迟、对齐检查、I/O 中断和反初始化入口。
- 缺少 `upper_hal_sdmmc` controller/OS/DMA/IRQ 实现时，Host 函数指针保持为空，桥接层仍返回 `-ENOSYS`，避免候选配置在链接阶段产生强未定义 Host 符号。
- 事务提交前显式检查四字节 DMA 对齐，并在 HAL 提供 `check_buffer_alignment` 时调用它；不满足条件返回 `-EINVAL`。没有复制或伪造缓冲区，也没有宣称 DMA 已接入。
- 未修改默认配置，未把 ESP-IDF `upper_hal_sdmmc` FreeRTOS/队列/中断/DMA 源文件强行接入默认镜像。

## 离线验证

使用现有 `esp32s31-xts-flat-usb-adb/compile_commands.json` 的锁定 S31 RISC-V 编译器，临时加入：

```text
CONFIG_ESP32S31_SDMMC=1 CONFIG_MMCSD=1 CONFIG_MMCSD_SDIO=1
CONFIG_SDIO_BLOCKSETUP=1 CONFIG_IDF_TARGET_ESP32S31=1 -Werror
```

并加入锁定 HAL 的 `upper_hal_sdmmc`、`upper_hal_sd_intf`、`esp_hal_sd`、ESP-IDF `components/sdmmc`、HAL/SoC 头文件目录：

```text
SD1762_BRIDGE_WEAK_DMA_OBJECT_RC=0
SD1762_UNDEFINED_SYMBOLS_RC=0
```

`riscv32-esp-elf-nm -u` 显示所有 `sdmmc_host_*` 依赖为 `w`（weak），剩余未定义项仅为 NuttX 的 `memcpy`/`memset`、`nxsem_*` 和 `mmcsd_slotinitialize` 上层/系统符号；无强未定义 `sdmmc_host_*`。`git diff --check` 通过。

上述命令只生成 `/tmp` 对象，没有构建、刷写、串口访问或硬件调用。

## 尚未闭合

锁定 HAL 的 controller、FreeRTOS/OS 同步、DMA descriptor、SDMMC IRQ 和 NuttX 中断/事件适配仍未注册；没有 `/dev/mmcsd0`、CID/CSD、容量、读写、FAT 挂载、掉电恢复或外接卡日志。因此 SD 卡项目仍为开发候选，不增加 xTS 或比赛清单的通过数。
