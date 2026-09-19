# SDMMC 真实 controller 的 NuttX 同步接入与完整链接

日期：2026-09-19。仅主机代码/构建验证；没有刷板、串口操作或外接卡测试。

## 实现

新增 `arch/risc-v/src/esp32s31/esp32s31_sdmmc_os.c`，为锁定 SD host 实现所需的实际同步：

- mutex 使用 NuttX `nxmutex`，支持零等待、有限 tick 超时及无限等待；ISR 不能操作任务 mutex。
- binary semaphore 使用 `nxsem`，初始为空，给出上限为 1；等待超时返回失败，不伪造事件。
- 有界队列预分配存储，IRQ-safe spinlock 保护 FIFO 及回绕；ISR 复制完整事件、满队列返回失败；接收等待真实 semaphore 通知，有限超时不改输出缓冲。
- 延时使用 NuttX tick 转换与 `nxsig_nanosleep`，按负 `-EINTR` 语义继续剩余延时。NuttX 在 IRQ semaphore post 后自行安排中断返回调度，`portYIELD_FROM_ISR` 不再调用另一个调度器。
- caps 分配复用工程现有 `heap_caps_calloc/free`；互斥锁复用已有 `sys/lock.h` / `esp_libc_stubs`，不新建空锁。限制 SDMMC 配置必须使用整毫秒 tick，防止锁定源的 `portTICK_PERIOD_MS` 除零或精度截断。

新 `esp32s31_sdmmc_idf_compat.h` 只针对原 SD host 对象补显式系统锁/延时声明及 `__containerof`。原 SDK/HAL 文件未改。

## 真实控制器接入

新增隔离 profile `xts-flat-sdmmc-host` 和 `ESP32S31_SDMMC_IDF_HOST`。在该配置下：

- 桥接层禁用旧 `#pragma weak sdmmc_host_*`，改用必须解析的强依赖。
- 编入锁定 `sd_host_sdmmc.c`、`sd_trans_sdmmc.c`、legacy host/transaction、通用 host 接口、SDMMC HAL 和 S31 slot/peripheral 定义。
- 完整链接复用已有真实 GPIO、IRQ、heap/cache 平台实现：`nuttx.map` 明确从 `sd_host_sdmmc.c.o` 拉入 `gpio.c.o` 和新同步对象；现有 NuttX platform IRQ 路径通过 `esp_setup_irq_with_flags_intrstatus` 配置中断。
- CMake 和 Make 文件均接入相同 source 集；本次实际验证的是 CMake/Ninja 构建。

## 验证与证据

1. `compile-sd-host-os.py` 对四个真实锁定 controller/transaction 源及新 NuttX 同步源执行 `-Wall -Werror` 交叉编译，全通过，见 `sd-host-os-compile.log`。
2. 中间 `ld -r` 组合真实 HAL 与 OS 对象通过，11 个 Queue/Semaphore/Delay 依赖解析为强 T 符号，见 `sd-host-os-combined-symbols.log`。
3. `test-sd-os-sync.py` 编译实际新 OS 源，以 pthread/sem 替代最低层 NuttX 原语进行主机 ASan/UBSan 回归。验证非法参数/分配失败、满空队列、复制/FIFO/回绕、有限超时、binary 上限、ISR mutex 拒绝、10000 个并发事件及延时通过，见 `sd-host-os-sync-test.log`。该测试不模拟真实硬件 IRQ/DMA。
4. **全新输出目录完整链接成功**：`openvela-dev/out/esp32s31-xts-flat-sdmmc-host-os-20260919`。第一次发现 CMake source 属性作用域问题，修正为 `TARGET_DIRECTORY arch` 后，最终构建进程退出 0，日志 `logs/build-sdmmc-host-os.log`。
5. 镜像 365064 字节，SHA-256 `8cf4dd094191a6bcbe925c68b057804cb921f37b14491818bfb5ef96cbe6ab73`，回执 `build-sdmmc-host-os.sha256`。
6. `sd-host-os-final-symbols.log` 记录最终 ELF 中强符号 `sdmmc_host_init`、`sdmmc_host_do_transaction`、真实控制器/事务处理、`xQueueReceive`、`esp_intr_alloc`、`gpio_config` 等。该结果不是原来只链接弱 ABI 的 1775 镜像。
7. `git diff --check` 通过；锁定 `sd_host_sdmmc.c` / `sd_trans_sdmmc.c` 哈希与 1777 审计一致，旧冻结镜像未覆盖。构建脚本退出已恢复 source 配置。

复现：

```sh
python3 backups/2026-09-10-scan-stress/test-sd-os-sync.py
python3 backups/2026-09-10-scan-stress/compile-sd-host-os.py
bash backups/2026-09-10-scan-stress/build-sdmmc-host-os.sh /tmp/sd-host-new-receipt.sha256
```

## 未完成边界

目前完成真实 controller 编译/最终链接及同步行为主机验证，不能推断 GPIO20–25 接线、卡电压/供电、实际 IRQ 通知、DMA/cache 数据一致性已经在板上通过。后续仍需真实 SD 卡验证识别、容量、块读写、FAT 挂载与掉电行为；清单 SDMMC 不应标为实物测试通过。
