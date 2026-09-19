# I2C/SPI BMI160 候选构建边界（1791）

日期：2026-09-19。范围是清单 `1.3.7 I2C/SPI 功能测试` 的离线准备；未刷写、未占用串口、未连接 BMI160，不能作为目标板 PASS。

## 已验证

- 重新运行 `check-bmi160-host.py`，实际 `bmi160_getregs`、`bmi160_read` 和 `bmi160_register` 函数的 ASAN/UBSAN 主机检查通过，覆盖有符号轴数据、24 位 sensor time、短/长缓冲区、I2C 错误、芯片缺失、注册失败和成功注册。
- `xts-flat-bmi160` 独立 profile 的 CMake 配置已进入 NuttX `olddefconfig`；此前由 SDMMC 选项引入的 `ARCH_HAVE_SDIO -> ESP32S31_SDMMC -> MMCSD_SDIO -> ARCH_HAVE_SDIO` 依赖回环已修复。`ESP32S31_SDMMC` 现在按 NuttX Host 驱动惯例选择 `MMCSD`、`MMCSD_SDIO` 和 `ARCH_HAVE_SDIO`，默认仍为关闭。
- profile 的预期硬件配置保持不变：I2C0、SCL GPIO45、SDA GPIO46、BMI160 地址 `0x68`、字符设备 `/dev/accel0`，未启用 uORB 或 SPI 变体。
- 新增 `tools/esp32c6-bmi160-emulator` 等效夹具，使用锁定 ESP-IDF 6.1 的 C6 I2C 从机 API，默认 GPIO4/5、地址 `0x68`，实现芯片 ID、配置写入、15 字节六轴/24 位时间读取；最小组件构建通过，固件 SHA-256 为 `f442adc3011bfe611c44d3c6e78210e9ef9882643474660e5b128946de7ebf26`。

## 当前阻断

第二次独立构建在 S31 现有 CMake 工具链适配器阶段停止，错误为生成的编译选项 `-march=if` / `-mabi=f` 无效，尚未进入 BMI160 目标编译。该错误属于当前工程已有的 S31 CMake adapter/toolchain 配置问题；本轮没有绕过它，也没有把主机检查当作目标板通过。

仍需在适配器修复后完成候选镜像构建，并由外接 BMI160 按原始 100 次读取用例完成 I2C 电气、芯片识别和数据记录。当前清单状态保持“待实物”。

## ESP32-C6 辅助板边界

当前工程包含 `esp32c6-devkitm` 的 I2C、SPI 主机和 SPI 从机配置。C6 可用于线序、3.3 V 电平、起始/停止、片选和收发路径的辅助检查，也可作为 SPI 对端或总线流量发生器做协议级冒烟测试。

这些结果只能记录为辅助验证，不能替代 S31 运行 xTS。新增模拟器可以替代真实 BMI160 完成“功能等效”运行，但仍需刷入 C6、完成两板接线，并在 S31 上实际获得芯片 ID、无错误 I2C 传输和原始 100 次读取，才能记为等效通过；真实器件验收仍需 BMI160 模块接到 S31 I2C0 GPIO45/46。

## 正式 BMI160 接线

| ESP32-S31 Function-CoreBoard-1 | BMI160 模块 |
|---|---|
| J2.15 / GPIO45 | SCL |
| J2.16 / GPIO46 | SDA |
| 3.3 V | VDD、VDDIO |
| GND | GND |
| 3.3 V | CSB（选择 I2C 模式） |
| GND | SDO（地址 `0x68`） |

SCL/SDA 必须有 3.3 V 上拉；若模块没有上拉，分别外接约 2.2--4.7 kOhm。不要接 5 V。正式读取时不要把 C6 并接到这两根 I2C 线上，以免引入第二个主机或改变总线负载。
