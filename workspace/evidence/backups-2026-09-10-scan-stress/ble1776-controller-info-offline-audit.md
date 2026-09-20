# 1776 BLE controller HCI 版本采集离线审计

日期：2026-09-19  
范围：核验集中的用户自添加第 8 项（BLE 5.4）及其 controller 证据准备。  
结论：本记录只证明采集路径已进入隔离镜像并可复核，不证明 controller 为 BLE 5.4，也不计任何 xTS 通过。

## 可复核输入

1747 隔离镜像的收据与当前文件逐项 SHA-256 一致：

```text
f74db48a0f23a532474af41a2fd2d0b78da39bb8d50e21795d4609efb5e6d27f  openvela-dev/out/esp32s31-ble-controller-info1747/nuttx.bin
74f0e7423afc32dc17e5bc513a035e94981755832f9cac470bcae00673e9fcdf  openvela-dev/out/esp32s31-ble-controller-info1747/appfs.img
```

对应构建配置仍为：

```text
CONFIG_ESP32S31_BLE=y
CONFIG_BT_EXT_ADV_MAX_ADV_SET=2
CONFIG_BLUETOOTH_TOOLS=y
```

## 采集链路审计

`hci_core.c` 的实际构建对象为：

```text
openvela-dev/out/esp32s31-ble-controller-info1747/apps/external/zblue/CMakeFiles/zblue.dir/zblue/subsys/bluetooth/host/hci_core.c.o
SHA-256 c756ebf488d4173cfed824af704edb56164812df5b69a86261cca530df816af7
```

源码在 `bt_dev_show_info()` 中通过 NuttX `syslog` 输出 HCI Core Version、revision、manufacturer 和 8 字节 LE Features；`bt_finalize_init()` 在 controller 初始化完成后调用该函数。对象的 `.rodata` 中实际包含格式字符串 `S31 BLE controller HCI=... LEfeat=...`，说明日志路径已进入 1747 构建，而不是只存在于未使用源码。

1758 采集脚本的离线检查仍通过：

- `python3 -m py_compile backups/2026-09-10-scan-stress/ble1747-controller-info-capture.py`
- `--help` 可执行；脚本拒绝覆盖已有日志/结果；
- 启动后先创建 `/data`、挂载 tmpfs、创建 `/data/misc/bt`，再执行 `bttool`；
- 只接受同时出现完整 `S31 BLE controller HCI=... LEfeat=...` 与新提示符的异步输出；
- 启动、目录准备、HCI 等待任一步失败均保留 UART 原始日志，绝不合成版本结果；成功路径执行 `disable`/`quit` 清理。

## 当前边界

本轮没有打开 UART、刷写、复位或连接 BLE 对端。历史 1747 运行记录因缺少 `/data/misc/bt` 在既有回调断言处停止，不能替代修正脚本的运行结果。因此 `runtime_hci_version` 仍为 `null`，BLE 5.4 继续保持“controller 待实物核验”；host 的默认 5.4 配置、芯片 `BT 5.4 (LE)` 字段和本次镜像字符串均不能单独作为版本通过。

后续恢复实物条件后，应使用收据绑定的 1747 镜像运行 1758 脚本，保存完整 `uart.log` 和 `result.json`；只有记录到 HCI 版本/LE Features 且完成清理，才可更新第 8 项状态。
