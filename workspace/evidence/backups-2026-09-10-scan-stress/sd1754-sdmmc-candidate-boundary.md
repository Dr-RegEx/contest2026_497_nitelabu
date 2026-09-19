# SDMMC 最小候选边界（1754）

日期：2026-09-18。此记录用于界定下一轮隔离 profile 的可编译入口，不代表 SD 卡协议通过，也不改变当前默认镜像。

## 当前结论

- 当前 Function-CoreBoard 生产配置仍关闭 `CONFIG_MMCSD`，未启用 `CONFIG_MMCSD_SDIO`，也没有 `CONFIG_ESP32S31_SDMMC` 板级配置。
- S31 SDK/HAL 的 SDMMC 能力已存在，但 HAL 的 `upper_hal_sdmmc` 依赖 `upper_hal_sd_intf`、SD 协议类型、DMA/中断、缓存和 FreeRTOS 兼容接口；这些源文件尚未接入 NuttX 的 S31 构建图。
- NuttX 的 `struct sdio_dev_s` 需要完整命令、响应、数据传输、事件等待和 DMA 方法表。当前不存在把 Espressif `sdmmc_command_t` 映射到该 ABI 的 S31 实现。
- 因此本轮没有启用配置、没有刷写、没有注册 `/dev/mmcsd0`，也没有把静态 HAL 头文件当作实物通过证据。

## 隔离 profile 入口

后续实现应单独建立 SD profile，至少显式打开：

```text
CONFIG_MMCSD=y
CONFIG_MMCSD_SDIO=y
CONFIG_ESP32S31_SDMMC=y
```

该 profile 应只使用 slot 0 的固定 IOMUX：`D0=GPIO20`、`D1=GPIO21`、`D2=GPIO22`、`D3=GPIO23`、`CLK=GPIO24`、`CMD=GPIO25`。板级启动顺序应为：

1. 初始化 S31 SDMMC Host 和 slot0；
2. 将 Host 事务封装为 NuttX `sdio_dev_s`；
3. 调用 `mmcsd_slotinitialize(0, dev)` 注册 `/dev/mmcsd0`；
4. 在外接卡实物上记录 CID/CSD、容量、单块/多块读写和 FAT 挂载日志。

## 当前编译边界

默认 profile 的静态检查通过：生产 `defconfig` 仍明确 `# CONFIG_MMCSD is not set`，S31 架构目录没有新增 SDMMC 源文件或默认链接项。启用上述隔离 profile 前，不能宣称桥接可编译或可运行；下一步需先补 FreeRTOS/NuttX OS 兼容入口及 `sdio_dev_s` vtable，再做独立配置构建。

