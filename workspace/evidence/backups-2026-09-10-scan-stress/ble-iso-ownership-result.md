# LE Audio：ISO TX 所有权与 H4 失败清理

2026-09-19，清单 IV9 主机修复；无刷板、串口或无线/音频实测，不新增实物通过项。

## 确定问题及实现

1. **ISO TX H4边界和异步所有权错误。** 原 s31_send_core 分配带 H4 包，经锁定 hci_driver_standard.c 的 ISO case 原样传指针而 length 减1；下游 controller 期望无 H4 的 ISO header+payload，并保留传入指针，桥接返回立即 kmm_free 造成 UAF。现在 ISO 独立经 btdm_osal_malloc 分配无 H4 的包，持 g_lock 检查 open 后直接调用锁定 ble_hci_trans_hs_iso_tx，移交所有权。关闭态未移交则本地 btdm_osal_free。非 ISO 原路径保留；CONFIG_BT_ISO 未启用时 ISO 显式 ENOTSUP。
2. **H4 send失败双释放。** 实际链接的 frameworks/connectivity/bluetooth/service/stacks/zephyr/hci_h4.c 原来失败也 net_buf_unref；host hci_core.c 错误路径再次 unref。现在成功才消费，失败恢复 H4 prepend 并由 caller 释放，保留真实 errno；正数 partial write 继续补写，write=0 返回 EIO，不再无限循环，EINTR 重试。
3. send 读取传入 dev->data，不再错误地从全局 bt_dev 选择设备。仅修发送设备选择，不声称完成多控制器 service-loop 改造。

## 锁定 controller ABI 的实际证据

分析对象为之前冻结并实际链接的 `out/esp32s31-ble-iso-controller-fix/nuttx`，不是其它芯片示例。反汇编留存 `ble-iso-controller-ownership-disassembly.txt`。

- r_ble_hci_trans_hs_iso_tx（0x4008fcd6）转调 ble_hci_trans_env_p+64；r_ble_iso_trans_cfg_ll 正是写此字段。初始化调用点 0x4004ddde 注册 `r_sym_ble_Iz1Chq2XDIpX4iL8g2ft`（0x40062cde）。
- 该真实 controller callback 在 0x40062d36 将原指针 s1 存入队列节点，不复制包，证明调用返回后仍需存活。
- 拒绝分支 0x40062db6 以原指针调用 `r_sym_ble_M1vTuH91lR6M04XZ7p4T` 再返回错误。后者实际是 libble_app.a(34.o) 的 IRAM 函数（0x2f0054b6），并非不可见 ROM；反汇编跳转 ext_funcs 表+8。
- 当前 esp32s31_ble_controller.c 的 ext_funcs_t 偏移8为 `_free`，ext_funcs_ro 绑定 btdm_osal_free；esp32s31_ble_osal.c 的 wr_btdm_osal_malloc/free 使用 kmm_malloc/free。新代码显式调用相同 OSAL allocator，匹配 controller 释放，成功或 controller 拒绝之后都不二次释放。
- 锁定 esp_ble_iso/host/common/iso.c 对错误路径也说明 controller 释放；NimBLE示例有不同错误清理写法，本修复依据上述实际ELF，未照搬示例。

## 验证与边界

- `tools/tests/ble-iso-bridge/run.py`：完整生产桥接源，FLAT/KERNEL 两种；ASan+UBSan；ISO无H4包、控制器消费后禁止二次释放、错误返回、关闭态释放、300字节/14位上限/零payload及长度RFU拒绝、CMD/ACL回归。控制器替身模拟已核实所有权，不模拟无线成功。
- `tools/tests/ble-h4-send/run.py`：实际生产 h4_send_data/h4_send；ASan+UBSan；两个不同 dev、ISO成功、ENOSPC、zero write、EINTR、closed fd；失败后buffer仍caller拥有且header复原。
- 现有RX保留ISO类型分离：S31 BT_ISO_IN → uart_bth4 H4_ISO → framework get_rx BT_BUF_ISO_IN；框架按 tailroom 拒绝超出接收池的包。没有把ACL pool当ISO，也未更改RX controller allocator契约。
- 两个修改 C 文件按最终生成config优先include并加 `-Werror` 严格交叉编译通过。记录 `ble-iso-ownership-strict.log`。
- 新目录完整构建 `build-ble-iso-ownership-fix.sh` 退出0；源 `.config`/`config.h` 已恢复，旧镜像保留。

旧profile桥接验证镜像（保留，非最终完整功能镜像）：`openvela-dev/out/esp32s31-ble-iso-ownership-fix/nuttx.bin`

SHA256：`38d3a82627b5440e5c2d3fb4ce78b0c031e6623207b9e875b8de40ec9cd2da91`

构建日志 `build-ble-iso-ownership-fix.log`；回归日志 `ble-iso-ownership-host-test.log`；源码/配置/镜像哈希 `ble-iso-ownership-fix.sha256`。

仍需真板验证开启ISO后的controller初始化、能力、CIS/BIG及BAP对端收发、实际音频。没有修改原SDK、控制器二进制或任何版本/feature返回值，没有改主清单。

## 最终四角色 + LC3 集成候选

按最新 `xts-flat-ble-audio-shell` 独立全构建 `build-ble-audio-lc3-iso-ownership.sh`，退出0，源配置恢复、构建锁释放。包含本次两项修复，未回退旧profile。

- 配置确认：BAP Unicast Server/Client、Broadcast Source/Sink 四角色全部 y；BT_SHELL=y、LIBLC3=y。
- 实际最终 ELF 包含 bt_bap_unicast_server_register_cb、bt_bap_unicast_client_discover、bt_bap_broadcast_source_create、bt_bap_broadcast_sink_create、lc3_encode、lc3_decode、shell_cmd_bap。符号证据 `ble-audio-lc3-iso-ownership-symbols.txt`。
- 最终镜像 `openvela-dev/out/esp32s31-ble-audio-lc3-iso-ownership/nuttx.bin`。
- SHA256 `56e90cd95e41a3358e4320aa6b6549ae6e3bee1a8c666a9691c3ad69024f1a4d`。
- 最终源码/配置/镜像收据 `ble-audio-lc3-iso-ownership.sha256`，构建日志 `build-ble-audio-lc3-iso-ownership.log`。
- 两修改C再按该最终四角色配置严格-Werror编译通过，日志 `ble-audio-lc3-iso-ownership-strict.log`。

上述仍仅链接/代码验证，不等于四角色实际互通或音频通过。
