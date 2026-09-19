# SDMMC 初始 1-bit 与运行总线宽度修复

2026-09-19，清单 IV11。仅主机验证及完整构建，未刷板、未连接真实 SD 卡。

## 确定阻断

真实 NuttX `mmcsd_probe()` 先调用 `mmcsd_removed()`，后者通过 `SDIO_WIDEBUS(false)` 要求恢复 1-bit。S31 桥接将其传给 `sdmmc_host_set_bus_width(slot, 1)`。

实际链接 HAL 的 `sd_host_slot_sdmmc_configure()` 原来只把 width_state 设为 READY，未保存请求宽度。随后 `sd_host_slot_set_bus_width()` 直接使用槽位接线最大宽度（本板4），因此连第一次探卡前的 1-bit 请求也会被设置成4-bit。命令通路正常不代表 SCR 等初始化数据传输正常。

## 修改

维护的 HAL fork 中增加 `active_width`，与槽位最大接线宽度分离：

- 初始 active_width 为1；configure 保存明确请求的1/4/8，0表示不变。
- 拒绝非法宽度及超过已配置接线能力的请求，且不部分修改其它配置。
- 寄存器选择使用 active_width；槽位能力查询及GPIO准备仍保持原最大接线宽度。
- 采样模式的8-bit限制随实际选择宽度判断。

只改维护的 `deps/esp-hal-3rdparty`，不改原始 `tmp/esp-idf-clean`。私有slot结构发生变化，本轮已全量重编译所有相关HAL对象，不能把新旧HAL对象混合链接。

## 验证

- `python3 tools/tests/sdmmc-bus-width/run.py` 执行实际 configure 和 set_bus_width 函数，底层LL写寄存器由可观察替身提供。ASan/UBSan覆盖默认1-bit、首次probe恢复1-bit、1→4→1、width=0不变、非法2和超出接线8拒绝、保留最大能力，以及8-bit槽位选择4/8。
- 同一回归运行于旧源码，在首次明确要求1-bit的寄存器断言失败；修改后通过。`sd-bus-width-before.log`、`sd-bus-width-after.log`及旧源码 `sd-host-before-width-fix.c` 保留。
- `build-sdmmc-bus-width.sh` 新目录完整构建1341/1341，exit0；HAL沿用 `-Wall -Werror`，`git diff --check`通过，source配置已恢复。
- 镜像 `openvela-dev/out/esp32s31-xts-flat-sdmmc-width-20260919/nuttx.bin`，365724字节，SHA256 `2592a57fa636c718fb9849cd6c7f649ca581ebc28595c9e79f67081676f6ff3c`。
- 收据 `sd-bus-width.sha256`，日志 `logs/build-sdmmc-bus-width.log`。完整HAL累计补丁 `sd-hal-width-and-error.patch` 包含本次宽度修复及上一轮事务错误保留，供干净HAL checkout复现。
- F.0依赖核验：SDK和参考NuttX/Apps通过；HAL摘要因有意补丁不匹配，日志 `sd-bus-width-dependencies.log`。未刷新旧锁，不声称依赖锁全通过。

仍待实物验证：卡识别/SCR/CID/CSD、真实1/4-bit读写、DMA/cache/IRQ与文件系统。模拟寄存器选择不能替代这些结果，不增加xTS通过数。
