# SD1760 NuttX SDMMC bridge source compile

日期：2026-09-18。此记录只证明当前 `esp32s31_sdmmc.c` 候选能够在锁定的 S31 RISC-V 编译环境下通过 C 编译；不代表 SD 卡初始化、协议、块设备、FAT 或掉电恢复已经通过。

## 核验范围

源文件：

`openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_sdmmc.c`

编译使用现有 `esp32s31-xts-flat-category-fs-worker-1690/compile_commands.json` 的 S31 交叉编译参数，并在命令行临时加入：

```text
CONFIG_ESP32S31_SDMMC=1
CONFIG_MMCSD=1
CONFIG_MMCSD_SDIO=1
CONFIG_SDIO_BLOCKSETUP=1
CONFIG_IDF_TARGET_ESP32S31=1
```

同时加入锁定参考工程中的 `upper_hal_sdmmc/legacy/include`、`upper_hal_sdmmc/include`、ESP-IDF `components/sdmmc/include`、`upper_hal_sd_intf/include` 和 `esp_hal_sd` 头文件目录。命令使用 `-Werror -Wno-undef`，结果为：

```text
SD1760_BRIDGE_COMPILE_RC=0
text data bss dec hex
1050  344  0   1394  572  /tmp/s31-sdmmc.o
```

临时宏和 include 目录没有写入仓库配置，也没有生成或刷写镜像。

## 集成边界

对象仍有完整 HAL 和 NuttX 上层的外部符号，包括 `sdmmc_host_*`、`mmcsd_slotinitialize` 以及同步原语；当前源码尚未接入 arch 的 Kconfig/Makefile/CMake 默认路径，也没有把 HAL 实现依赖加入独立镜像。因此这一步只能记为“bridge 候选源级编译通过”。

尚未完成：外接 SD 模块供电、上拉和线序核验，Host 初始化，CID/CSD 识别，块读写，`/dev/mmcsd0` 注册，FAT 挂载和掉电恢复。用户无法人工干预期间不执行串口、刷写或实物测试。

## 结论

SD 卡协议项目仍为待适配/待实物验收状态；本记录仅为后续独立 profile 集成提供编译基线，不增加 xTS 或比赛清单的通过数。
