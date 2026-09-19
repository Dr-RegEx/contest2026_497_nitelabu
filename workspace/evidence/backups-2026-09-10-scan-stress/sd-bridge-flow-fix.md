# SDMMC 桥接命令/数据/事件流程修复

日期：2026-09-19。仅主机；没有刷板、串口或真实 SD 卡测试。

## 修复的实际阻断

对照当前 `drivers/mmcsd/mmcsd_sdio.c` 的真实调用顺序，原桥接有以下确定问题：

1. `recvsetup/sendsetup` 写入 command 的 data 指针，随即被 `sendcmd` 的 memset 清空。现在待传输缓冲独立保留；每次数据命令消费且完成后清除，不会附着到后续无数据命令。
2. 原 capability 未声明 `SDIO_CAPS_DMABEFOREWRITE`，上层会先发 CMD24/25 再 setup，但锁定 HAL 采用一次同步命令+数据事务。现在声明该能力，使上层先 setup 再发写命令；不额外声明未提供的 NuttX DMA callbacks。
3. 原隔离配置未启用 `SDIO_BLOCKSETUP`，多块传输缺少块长/块数。现由 SDMMC Kconfig select；验证数据长度与块长/块数匹配，避免错误 descriptor。
4. 原 `timeout_ms` 为零，`waitenable` 忽略超时。现在命令默认 1000ms，数据事务使用上层配置超时，最少一个真实 tick，避免锁定 HAL 整除为零。
5. 原 status 永远为零，NuttX 不会尝试识别卡。按 `mmcsd_hwinitialize` 对无 CD 连线的 polling host 约定设置 PRESENT，让命令探测决定是否有卡；这不代表已检测到真实卡。
6. slot 初始化失败重试时不再重复创建已成功初始化的 controller。
7. R2 从 IDF 低字在前转换为 NuttX 高字在前，否则 CSD/CID 解码错误。
8. 同时检查 HAL 返回码与 `command.error`，正确传播 CRC、超时、内存/参数错误。eventwait 只报告真实完成的受关注事件或错误，无执行/重复等待不再伪造 TRANSFERDONE。

## DMA 缓冲所有权

- 根据真实 internal cache alignment 分配带 `MALLOC_CAP_INTERNAL|DMA|8BIT` 的对齐 bounce buffer；保留真实数据长度，分配长度向上取整。
- 写入先复制调用方数据；读取仅在 HAL/command 同时成功后复制回，错误不写坏调用方缓冲。支持普通上层未对齐缓冲。
- 继续调用真实 HAL alignment 检查，不默认认为任意地址可 DMA。
- 同步事务返回后显式停止 IDMAC，覆盖锁定 HAL 某些早期命令启动失败路径；最多等待 reset 自清除 100ms。
- reset 超时则保留 DMA buffer，不释放或重用仍可能由硬件持有的内存；后续命令在 reset 未完成时返回 EBUSY，确认完成后才回收。此恢复机制未实板验证。

## 验证

- `test-sd-bridge-flow.py` 提取并执行当前生产桥接函数，使用可观测假 HAL 注入返回结果；ASan/UBSan 通过。覆盖双块读写、setup 不丢失、不对齐调用方、bounce 复制、无数据命令不带旧缓冲、R2 字序、timeout/CRC/参数/分配失败、无假完成、初始化重试、stuck DMA reset 保留与恢复。日志 `sd-bridge-flow-test.log`。
- 全新目录 `openvela-dev/out/esp32s31-xts-flat-sdmmc-flow-20260919` 完整 CMake/Ninja 构建退出 0，原冻结镜像未覆盖；配置确认 `CONFIG_SDIO_BLOCKSETUP=y`。
- 镜像 365676 字节，SHA-256 `f3875c9e4dbd72dab854d67e94df838c4d0274418f3316080173c1e910ec5183`。
- 回执 `build-sdmmc-flow.sha256`；构建脚本 `build-sdmmc-flow.sh`；日志 `logs/build-sdmmc-flow.log`。
- 冻结新配置对修改桥接源追加 `-Wall -Werror` 严格交叉编译通过；`git diff --check` 通过。源配置已恢复。

## 边界

主机测试注入的是 HAL 结果，不是虚构卡读写成功；不会增加真实 SD 测试通过数。GPIO/实际卡识别、IRQ/DMA 时序与 cache 一致性、FAT 和掉电行为仍需板上真实 SD 卡。当前是无 CD 线的轮询方案，没有宣称热插拔回调支持。
