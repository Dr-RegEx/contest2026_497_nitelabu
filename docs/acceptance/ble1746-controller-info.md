# 1746 BLE 控制器版本证据候选

日期：2026-09-18

## 目的

为用户自添加第 8 项 BLE 5.4 建立可复核的控制器证据入口。host 层默认的 BLE 版本宏不能替代控制器 HCI 实际上报值，因此在 `bt_dev_show_info()` 增加一条 NuttX UART `syslog`，记录 HCI core version、revision、manufacturer 和 8 字节 LE Features。

## 验证结果

- 当前开发板 `esptool chip-id` 读取成功，芯片能力字段显示 `Wi-Fi 6, BT 5.4 (LE)`，MAC 为 `30:ED:A0:F3:F7:B0`。这是芯片/ROM 能力声明，不等于当前加载 controller 的 HCI Core Version。
- 新增日志代码位于 `openvela-dev/apps/external/zblue/zblue/subsys/bluetooth/host/hci_core.c`。
- 使用 `demo-rmt-bttool-kernel` 和已经成功构建的 `demo-rmt-bttool-coex-multi` 两套实际交叉编译命令单独编译 `hci_core.c`，均编译成功。
- 1746 完整镜像构建未闭合：该 profile 的既有网络配置在 `lib_getifaddrs.c`、`lib_indextoname.c`、`lib_nametoindex.c` 触发 `SIOCGIFNAME/SIOCGIFINDEX/SIOCGIFFLAGS` 未定义；这不是新增日志代码的错误，且未修改该无关基线。
- 因没有完整镜像和刷写，本记录没有控制器运行时 HCI 版本结果，也不改变 BLE 5.4 状态。

## 1747 运行时尝试

修正隔离构建脚本后，1747 完整镜像成功链接并生成配对收据，随后短时刷写开发板。启动进入 NSH 正常；执行 `bttool` 时因镜像未先创建 `/data/misc/bt`，在既有 `callbacks_list.c:82` 断言处复位，未进入 controller 初始化，因此没有 HCI 版本日志。原始 UART 记录保存在 `ble1747-controller-info/uart.log`，诊断镜像随后恢复为 1743 多广播镜像。

## 结论

当前仍不能宣称 BLE 5.4 已完成。需要在准备好 `/data/misc/bt` 的可用 BLE 镜像中采集 UART 的控制器实际版本；若报告 5.2，则按控制器能力边界记录，不能由 host 5.4 默认值替代。
