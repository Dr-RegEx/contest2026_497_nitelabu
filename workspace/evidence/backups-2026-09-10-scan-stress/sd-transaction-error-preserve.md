# SDMMC 事务失败不再被缓存同步覆盖

2026-09-19。主机修复及构建完成，未刷板、未访问串口、未测试真实 SD 卡。

发现实际链接的 HAL `sd_host_slot_sdmmc_do_transaction` 在事件等待或 busy 等待超时后，仍执行 M2C 缓存同步，并把同步返回值赋给事务返回值。缓存同步成功会使事务超时变成 `ESP_OK`；事件等待超时还可能没有设置 `cmdinfo->error`，上层即可能把未完成的读缓冲复制给调用者。

修复位于维护的 HAL fork `s31-reference/deps/esp-hal-3rdparty/components/upper_hal_sdmmc/src/sd_trans_sdmmc.c`：用独立 `sync_ret`，已有事务错误优先；只有事务成功时才返回缓存同步错误。原始 `tmp/esp-idf-clean` 技术参考未改。

验证：

- `python3 tools/tests/sdmmc-transaction-error/run.py` 提取并执行真实完整事务函数；替身提供 HAL 事件、缓存及同步服务，不模拟真实 SD 成功。ASan/UBSan 覆盖事件超时、busy 超时、双重错误优先级、缓存失败、命令错误字段、成功、无缓存、无数据及提前失败。
- 同一回归运行于修改前源码，在“超时必须返回超时”的断言失败；修改后通过。记录 `sd-transaction-error-before.log`、`sd-transaction-error-after.log`，旧源码 `sd-trans-before-error-preserve.c` 保留。
- `build-sdmmc-error-preserve.sh` 在新目录完整构建 1341/1341，exit 0；HAL 编译沿用 `-Wall -Werror`。源码配置恢复，`git diff --check` 通过。
- 镜像 `openvela-dev/out/esp32s31-xts-flat-sdmmc-error-preserve-20260919/nuttx.bin`，365684 字节，SHA256 `f9d80ea2d5c508c933823e596a2119253bf169cac7accac1dae3dc43cba72ed8`。源码、配置、脚本、镜像收据 `sd-transaction-error-preserve.sha256`；完整构建日志 `logs/build-sdmmc-error-preserve.log`。

依赖核验已运行：原始 SDK、参考 NuttX、参考 Apps 均通过；HAL 原 F.0 lock 要求零补丁，因本次有意修复，其 patch digest 不再匹配。未改写旧锁来掩盖差异。完整修改保存为 `sd-transaction-error-preserve.patch`，日志 `sd-error-preserve-dependencies.log`；这不是依赖锁全通过，也不代表整个当前 openvela 工程由 F.0 锁覆盖。

复现新构建需包含此 HAL 补丁。实际 SD 卡识别、DMA/cache 时序、读写/FAT 仍待外接卡实测；本轮不增加 xTS 通过数。
