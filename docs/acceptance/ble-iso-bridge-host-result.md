# LE Audio：S31 HCI ISO 桥接修复（主机验证）

日期：2026-09-19。范围：比赛清单用户自添加第9项 LE Audio 的必要传输代码；未刷板、未开串口，不新增实测通过项。

## 确定缺口与修复

`nuttx/arch/risc-v/src/esp32s31/esp32s31_ble.c` 原先只接受 CMD/ACL 发送和 EVT/ACL 接收，ISO 数据会分别返回 `-EINVAL` / `-ENOTSUP`，即使 host BAP 已编译也不能传送音频 ISO 数据。本次增加：

- `BT_ISO_OUT` 到 `H4_ISO` 的发送，以及 `H4_ISO` 到 `BT_ISO_IN` 的接收。
- 校验四字节 ISO header、14位 payload 长度、保留位和实际长度；拒绝截断/超长帧。
- kernel address-environment dispatch 对 ISO 的复制上限从通用259字节改为协议最大16387字节（4+16383）。CMD/ACL 原上限保留。实际可发送长度仍受上层UART/ISO缓冲和控制器能力限制。
- 继续传递底层错误，不将发送失败记为成功。

NuttX `bt_buf.h`、`bt_uart.h`、`uart_bth4.c` 已定义和支持 `BT_ISO_IN/OUT`、`H4_ISO`。板级链接使用锁定 HAL `components/bt/porting_btdm`，其 VHCI driver 已有 ISO 路由（非另一套 `porting` 代码）。

## 验证

运行 `python3 tools/tests/ble-iso-bridge/run.py`：FLAT、KERNEL 两种模式通过。脚本编译整份生产桥接 C 文件，仅替换硬件/OS API 头及边界桩，检查300字节ISO双向数据一致、14位最大/零payload、截断和RFU拒绝、底层错误传播、关闭状态、CMD/ACL回归。使用UBSan。此测试不模拟无线成功，也不证明controller内存所有权、CIS/BIG或音频效果。

完整构建：`bash backups/2026-09-10-scan-stress/build-ble-iso-bridge-fix.sh`，1818步编译链接退出0；既有HAL/蓝牙框架警告记录在日志中。构建后源树 `.config` 和 `include/nuttx/config.h` 已恢复。

- 新镜像：`openvela-dev/out/esp32s31-ble-iso-bridge-fix/nuttx.bin`
- SHA256：`dc27ced445f3022d924bee81fe09bbd193dd9b11e83ac3cc65e3719e71f50e7f`
- 日志：`build-ble-iso-bridge-fix.log`、`ble-iso-bridge-host-test.log`
- 固件及修改源文件哈希：`ble-iso-bridge-fix.sha256`

旧1727冻结镜像未覆盖。最初直接使用旧compile_commands单文件验证因源树恢复配置优先于输出配置而缺少controller配置宏；已改用独立完整构建验证，未通过伪造宏规避。

## Controller ISO 配置与回调续修

继续修复后，上述“controller ISO未启用”代码阻断已消除：

- `esp32s31_bt_config.h` 按 `CONFIG_BT_ISO` 启用 `CONFIG_BT_LE_ISO_SUPPORT`，配置12个251字节controller ISO buffer，并映射host最大group/channel数。按锁定IDF Kconfig限制group为1–2、channel为1–6，越界编译失败。
- 不启用IDF默认可选NSFC；ZBlue使用标准HCI completed-packet流控，不调用Espressif私有NSFC查询API。
- 锁定 `porting_btdm/transport/src/hci_transport.c` 原ISO RX仅转发给IDF NimBLE/Bluedroid，外部NuttX host会被遗漏。保留原版权和实现至板级 `esp32s31_ble_hci_transport.c`，仅扩展ISO RX条件使NuttX使用实际VHCI driver回调；CMake链接该兼容副本。原参考SDK未改。
- 主机回归补充ISO关闭不启用controller、开启后的资源映射正确、非法7通道编译被拒绝，全部符合预期。

最终完整构建脚本：`build-ble-iso-controller-fix.sh`，日志 `build-ble-iso-controller-fix.log`，退出0。源树配置已恢复。

- 最终候选：`openvela-dev/out/esp32s31-ble-iso-controller-fix/nuttx.bin`
- SHA256：`86a8c563000e7a142ec5c9497d283b7b8acf8a5dde85d9ea53df7dc38489ceab`
- 哈希记录：`ble-iso-controller-fix.sha256`
- 实际ELF符号：`r_iso_stack_initEnv`、`r_iso_stack_enable`、`r_ble_hci_trans_hs_iso_tx`、`hci_transport_controller_le_iso_tx`均存在，控制器静态库无未解析ISO符号。

## 仍待完成

候选镜像未刷板。尚须实际controller初始化、读取开启ISO后的真实能力、CIS/BIG建立和BAP对端音频收发；当前只能标记“主机桥接及controller配置已修复、完整构建通过”，不能标记LE Audio实测通过。前一轮旧镜像真实feature值不是新镜像能力证明；本次不修改、填造任何版本或feature值。
