# Wi‑Fi 性能候选 1720：AMPDU BA=12（离线构建完成，待板测）

更新时间：2026-09-18。

## 依据

`nuttx/arch/risc-v/src/common/espressif/Kconfig` 对 Wi‑Fi AMPDU 参数的说明指出：
较大的 RX Block-Ack 窗口通常提高吞吐和兼容性；屏蔽箱 iperf 测试建议
`ESPRESSIF_WIFI_RX_BA_WIN` 使用 9～12，且静态 RX buffer 数量应不小于 RX BA
窗口。当前 1719 候选仍为 TX/RX BA=6、静态 RX buffer=10。

1720 独立配置继承 1719，并设置：

- `CONFIG_ESPRESSIF_WIFI_RX_BA_WIN=12`；
- `CONFIG_ESPRESSIF_WIFI_TX_BA_WIN=12`；
- `CONFIG_ESPRESSIF_WIFI_STATIC_RX_BUFFER_NUM=12`。

静态 RX buffer 从 10 增加到 12，按 Kconfig 约 1.6 KiB/个估算增加约 3.2 KiB
RAM；没有修改正式 SACK profile，也没有改变网络栈源码。

## 构建状态

1720 已完成离线构建，生成 `nuttx.bin` 和 `appfs.img`，并以
`build1720-wifi-ampdu12.sha256` 收据校验一致。配置确认 RX/TX BA=12、静态 RX
buffer=12、TCP 窗口因子3、IOB=128。该候选已刷入开发板并完成 5.1.22 的一次
300秒窗口测试，但板端摘要只覆盖286.86秒，结果记录为失败/部分证据，不能计入
xTS 通过项；5.1.23～5.1.25 尚待同一条件实测。

## 使用限制

候选仍需在同一 AP、同一主机及原始 300 秒流程下实测 5.1.22～5.1.25；在实测前，
清单四项继续保持“低于行业参考、需优化”。
