# SD 卡协议新增项目审计（用户自添加第 11 项）

更新时间：2026-09-18

## 结论

当前登记为“芯片能力和板级信号已确认、外接卡适配未开始”。本记录不构成 SD 卡协议通过，也不增加 xTS 通过数。

## 已确认的软件能力

- S31 SDK 配置头声明 `SOC_SDMMC_HOST_SUPPORTED=1`、两个 SDMMC slot、最大 4-bit 数据宽度、UHS-I 支持和 PSRAM DMA 能力：
  `openvela-dev/nuttx/arch/risc-v/src/esp32s31/include/sdkconfig.h`。
- 锁定的 S31 HAL 含 `upper_hal_sdmmc`，包括 `sdmmc_host_init()`、`sdmmc_host_init_slot()`、事务传输和 UHS-I/延迟配置：
  `openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp-hal-3rdparty/components/upper_hal_sdmmc/`。
- S31 HAL 固定 IOMUX 组为：
  - slot 0：CLK GPIO24、CMD GPIO25、D0..D3 GPIO20..23；
  - slot 1：CLK GPIO39、CMD GPIO40、D0..D3 GPIO35..38。
  来源：`components/esp_hal_sd/esp32s31/include/soc/sdmmc_pins.h`。
- 官方 Function-CoreBoard-1 用户指南确认 J2 引出了 slot 0 的 SDIO 信号：J2.26 GPIO20/SDIO_DATA0、J2.27 GPIO21/SDIO_DATA1、J2.28 GPIO22/SDIO_DATA2、J2.29 GPIO23/SDIO_DATA3、J2.30 GPIO24/SDIO_CLK、J2.31 GPIO25/SDIO_CMD。该板没有板载 TF/SD 卡座，需要外接模块；官方引脚表是硬件映射依据。
- NuttX 通用 `drivers/mmcsd` 目录存在 SPI/SDIO 协议层，但当前 Function-CoreBoard 的生产配置将 `CONFIG_MMCSD` 关闭；没有发现 S31 板级 `mmcsd` 注册或 `sdmmc_host_init_slot()` 调用。

## 当前缺口

1. J2 信号已由官方文档确认，但尚未确认外接 SD 模块的供电、上拉、电平转换和线序。
2. 尚未完成 NuttX 到 S31 HAL 的块设备桥接；单独启用 `CONFIG_MMCSD` 不能自动注册 SDMMC Host。
3. 尚无卡片初始化、CID/CSD/容量读取、单块/多块读写、文件系统挂载或掉电恢复日志。
4. 原始 xTS 精简测试集没有独立的 SD 卡协议编号；相关文件系统用例使用的是通用挂载路径，不能替代 SD 实物协议测试。

## 后续验收路径

准备外接 SD 模块并确认 J2.26–31 线序后，建立隔离 profile：注册 SDMMC Host 和 `/dev/mmcsd0`，完成卡初始化与容量识别，再做读写、FAT 挂载和一次受控掉电恢复。所有步骤都要保留 UART 日志、卡片型号和镜像收据；在此之前状态保持“待适配”。

硬件依据：<https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp32-s31-function-coreboard-1/user_guide.html>（J2 引脚表和板载组件表）。
