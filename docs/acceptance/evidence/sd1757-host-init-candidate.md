# SDMMC Host 初始化隔离候选（1757）

日期：2026-09-18。此记录只证明锁定的 S31 HAL 头文件能够表达 Function-CoreBoard-1 的 slot 0 配置，不代表 SD 卡初始化、协议、块设备或文件系统通过。

## 候选内容

`sd1757-host-init-candidate.c` 将 J2 的 slot 0 信号编码为 `sdmmc_slot_config_t`：`CLK=GPIO24`、`CMD=GPIO25`、`D0..D3=GPIO20..23`，4-bit，未假设卡检测/写保护 GPIO。它故意不调用 `sdmmc_host_init()`，也不注册 `/dev/mmcsd0`，因为当前 NuttX 构建还未链接 `upper_hal_sdmmc`、`upper_hal_sd_intf` 及其 OS/DMA 依赖。

## 离线验证边界

- 已使用 `sd1757-host-init-syntax.sh` 从现有 `esp32s31-xts-flat-usb-adb/compile_commands.json` 取得 S31 交叉编译参数，并补入锁定 HAL 的 `components/upper_hal_sdmmc/legacy/include`、`components/upper_hal_sdmmc/include`、`components/upper_hal_sd_intf/include`、`components/esp_hal_sd/include`、ESP-IDF `components/sdmmc/include` 及 S31 HAL `hal/soc` 头文件目录。命令实际输出：`SD1757_HOST_INIT_SYNTAX=PASS`。
- 验证命令：`backups/2026-09-10-scan-stress/sd1757-host-init-syntax.sh`。该命令只执行 `-fsyntax-only`，不生成镜像、不访问 UART、不调用 Host 初始化。
- 默认 `production`、`competition` 和现有 xTS profile 不启用任何 SDMMC 选项；不改变默认镜像、串口和板上运行状态。
- 完整下一步仍是：把 `sdmmc_command_t` 映射到 NuttX `sdio_dev_s` 的命令/响应/数据/事件方法，再把 Host HAL 源文件及其依赖加入独立 profile；随后才可调用 `mmcsd_slotinitialize(0, dev)`。

## 当前结论

该候选把板级引脚和 HAL 配置边界固定下来，属于“开发验证、待桥接和实物核验”。它不增加 SD 卡项目的通过数，也不能替代外接 SD 模块的 CID/CSD、读写、FAT 挂载日志。
