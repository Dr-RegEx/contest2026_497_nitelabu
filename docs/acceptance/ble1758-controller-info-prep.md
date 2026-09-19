# 1758 BLE 控制器版本采集准备修正（离线）

日期：2026-09-18

## 范围

本记录只针对“比赛必须适配清单”中的用户自添加第 8 项 BLE 5.4。它修正控制器 HCI 版本采集的启动准备，不构成 BLE 5.4 或任何 xTS 用例通过证据。

## 根因

1747 的独立控制器信息镜像能够启动到 NSH，但采集脚本直接执行 `bttool`，没有先创建 BLE 存储目录。此前成功的 BLE 实板流程已证明，启动 `bttool` 前需要执行：

```text
mkdir /data
mount -t tmpfs /data
mkdir /data/misc
mkdir /data/misc/bt
```

缺少该准备时，`bttool` 未进入控制器初始化/HCI 命令路径，随后在既有 `callbacks_list.c:82` 的回调注册断言处复位。该断言不是 BLE 5.4 能力证明，也不能通过关闭断言掩盖。

## 离线修正

`ble1747-controller-info-capture.py` 现在在每次硬复位后的 `bttool` 启动前执行上述四条命令，并把输出写入同一 UART 记录；若目录准备出现 NSH 错误，脚本立即失败而不生成伪造结果。进一步修正了三处证据边界：

1. 启动前校验两行镜像收据及 SHA-256，并拒绝占用中的 UART；
2. 不再把异步 `bttool>` 旧提示符当作完成，必须同时收到 `S31 BLE controller HCI=... LEfeat=...` 和新提示符；
3. 无论启动、目录准备或 HCI 采集失败都只写失败结果和完整 UART 日志，成功采集后执行 `disable`/`quit` 清理并记录清理状态。

脚本已通过 `python3 -m py_compile`、`--help` 和离线静态检查。1747 配对镜像的 `.config` 仍确认启用 `CONFIG_ESP32S31_BLE`、`CONFIG_BLUETOOTH_TOOLS`、`CONFIG_BT_EXT_ADV_MAX_ADV_SET=2`；`bttool` AppFS ELF 中包含新增 HCI 日志格式字符串。上述均是离线证据，不是实物 HCI 结果。

## 尚未完成

- 本轮用户无法人工干预，因此没有刷写或占用 `/dev/ttyUSB0`。
- 尚无修正后脚本的实板 UART 输出，不能填写 HCI Core Version、Revision、Manufacturer 或 LE Features；已有 1747 UART 记录仍只有启动到 NSH，不能替代采集结果。
- 即使后续采集成功，也只能证明当前 controller 的 HCI 能力；仍需与 BLE 5.4 要求及清单中的 BLE xTS 用例分别核对。

## 下一步

用户恢复操作后，使用已经配对的 `build1747-ble-controller-info.sha256` 镜像执行修正后的采集脚本；只有 UART 中出现完整匹配的 `S31 BLE controller HCI=... rev=... mfr=... LEfeat=...` 且保留完整日志、收尾回到 NSH 时，才更新 BLE 5.4 的控制器证据状态。
