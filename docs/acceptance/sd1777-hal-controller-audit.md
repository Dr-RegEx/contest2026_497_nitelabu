# SDMMC 真实 HAL controller 离线编译缺口审计（1777）

日期：2026-09-19
范围：只检查锁定的 ESP-IDF/esp-hal-3rdparty SDMMC Host controller 源能否在现有 NuttX 隔离 profile 中接入；没有刷写、没有打开串口、没有外接 SD 卡。

## 探测结果

1775 的 `xts-flat-sdmmc` 镜像只链接了 NuttX `esp32s31_sdmmc.c` 桥接和弱 `sdmmc_host_*` ABI。为确认能否继续接入真实 controller，使用 1775 的 `compile_commands.json`、同一 S31 GCC、同一 `esp32s31` 配置和锁定 `s31-reference/deps/esp-hal-3rdparty` 源，对以下对象做了离线语法/对象编译探测：

|锁定源|探测结果|第一处确定缺口|
|---|---|---|
|`components/upper_hal_sdmmc/src/sd_host_sdmmc.c`|未编译|`freertos/semphr.h: No such file or directory`|
|`components/upper_hal_sdmmc/src/sd_trans_sdmmc.c`|未编译|通过 `sd_host_private.h` 间接缺 `freertos/semphr.h`|
|`components/upper_hal_sdmmc/legacy/src/sdmmc_host.c`|未编译|`freertos/queue.h: No such file or directory`|
|`components/upper_hal_sdmmc/legacy/src/sdmmc_transaction.c`|未编译|通过 `sdmmc_internal.h` 缺 `freertos/queue.h`|

当前目录只有 `arch/risc-v/src/esp32s31/include/freertos/FreeRTOS.h`，它是 Camera DVP 使用的 `portMUX_TYPE`/临界区兼容层，不提供 Queue/Semaphore 类型或实现。因此把上述源码直接加入默认或隔离镜像会在编译阶段失败，不能把 1775 的弱符号链接误报为真实 controller 已接入。

## 运行时依赖边界

即使补齐头文件，锁定 Host 源仍需要实际 NuttX 适配，不能用空宏绕过：

- `sd_host_sdmmc.c` 创建和等待 `SemaphoreHandle_t`/`QueueHandle_t`，调用 `xSemaphoreTake/Give`、`xQueueReceive/SendFromISR`、`xSemaphoreGiveFromISR`、`vTaskDelay` 和 `portYIELD_FROM_ISR`；相关路径在 controller/slot 互斥、DMA/SDMMC ISR 及 SDIO 中断等待中。
- 该源通过 `esp_intr_alloc`/`esp_intr_free` 安装和释放 SDMMC ISR；NuttX IRQ attach/enable/disable/priority 语义尚未接入该 Host 上层。
- DMA 路径依赖 `heap_caps_calloc`、`esp_ptr_dma_capable`/`esp_ptr_dma_ext_capable`，并由 `sdmmc_ll_init_dma`、IDMAC descriptor、`SDMMC_LL_EVENT_DMA_*` 中断事件共同完成传输；现有桥接只做四字节对齐检查，不能替代 descriptor/IRQ completion。
- `esp_hal_sd/sdmmc_hal.c` 与 `esp32s31/sdmmc_periph.c` 只提供底层 HAL/slot 描述，不能独立提供 Host controller 的任务同步、ISR 投递和块传输状态机。

锁定源 SHA256（用于复核版本）：

```text
upper_hal_sdmmc/src/sd_host_sdmmc.c       1cc03e9dffac1629fc7422c01ff2673eaeb32e448f2b996833f99237a77725e6
upper_hal_sdmmc/src/sd_trans_sdmmc.c      7087f3f281886b13dacb90ee054a941efb831337cae2e8ebfb049a62c5a8841b
upper_hal_sdmmc/legacy/src/sdmmc_host.c   6e651f93cdd414e0ecfc630b6054876387633da92bc003acaf1444470f2e72f5
upper_hal_sdmmc/legacy/src/sdmmc_transaction.c d5d3b9185aac3f4b59a385d0138981a2e2c28911e7d1954c307606913c454d83
esp_hal_sd/sdmmc_hal.c                    8e4299cb049ee27e7973de1c644ff5dcd942af39351bb309cac015be8b972c2c
esp_hal_sd/esp32s31/sdmmc_periph.c        1235bd279378dec3bad8bb5a4993bff04c1cd68ecb916c38389d1f062dc34b6d
```

## 结论

本轮没有新增可合法链接的真实 controller 镜像，也没有增加 xTS 通过数。SD 卡协议仍保持“隔离桥接镜像已链接、真实 controller/OS 同步/DMA/IRQ/外接卡待补齐”的状态。下一步必须实现 NuttX 任务同步和 ISR/DMA 适配，再在隔离 profile 编译，最后由外接 SD 卡完成容量、块读写、FAT 挂载及掉电恢复实测；不应以弱符号或空 FreeRTOS 宏作为通过证据。
