# LE Audio ISO/BAP 应用入口：主机适配结果

日期：2026-09-19。只本地主机工作，未刷板、未打开串口，未增加实测通过数。

## 实现

新增独立 `xts-flat-ble-audio-shell` profile，复用锁定 ZBlue 的真实 `zblue`、`iso`、`bap` shell。此前音频profile关闭 `CONFIG_BT_SHELL`，只有协议栈静态链接，无法从命令行发起ISO/BAP操作。新profile使用8192字节入口任务栈；不改变默认profile。

为使既有入口可链接且缓冲池正确，修复：

1. `port/sections/defines.c` 的 `sine_tx_pool` 注册条件与其真实定义保持一致：仅BAP、LC3、Audio TX同时启用才引用，避免没有LC3时未定义符号。
2. `audio/shell/bap.c` 的 `tx_pool` 改名 `bap_tx_pool` 并更新注册表。原名字与 `host/shell/iso.c` 同名，在此移植的弱符号机制下会合并到同一池；新ELF中两池地址不同。
3. `include/zephyr/bluetooth/gatt.h` 补 `bt_gatt_resubscribe_mc` 声明及默认controller的inline兼容入口。实现已是multi-controller版本，原shell调用的旧声明没有对应可链接函数。

未改原始ESP-IDF SDK；这些是项目内ZBlue移植代码修复。

## 编译及入口证据

- 构建脚本：`build-ble-audio-shell.sh`
- 构建日志：`build-ble-audio-shell.log`，最终完整镜像编译链接退出0，存在既有依赖警告。
- 固件：`openvela-dev/out/esp32s31-ble-audio-shell-2/nuttx.bin`
- SHA256：`4c0bf46e8528faf6464dd26f1d5235ef72bbf0d8f08ebf8ca28aea15dfbcf5d7`
- 符号证据：`ble-audio-shell-symbols.log`，确认 `zblue_main`、`shell_cmd_iso`、`shell_cmd_bap`、真实BAP discover/connect/create_broadcast处理函数链接；ISO和BAP池地址不同。
- 源树 `.config`、`include/nuttx/config.h` 构建后已恢复。

后续上板入口为NSH输入 `zblue`，随后 `help`、`bt init`、`iso`、`bap` 查看真实支持命令。ISO提供connect/listen/send，BAP提供discover/config/qos/connect及broadcast相关命令；具体参数以该固件shell输出为准。需要独占蓝牙host，不应与运行中的蓝牙服务争用控制器。

## 验证边界和下一缺口

当前有效BAP角色为unicast client、broadcast source，未启用LC3，因此 `bap send` 的协议测试数据不能称为真实音频；需要codec入口或外部音频流后才能测音频播放。

尝试补server/sink依赖时发现：原profile虽然写了 `BT_BAP_UNICAST_SERVER/BROADCAST_SINK=y`，缺少GATT caching、ASCS/PACS、scan delegator依赖，最终生成配置并未真正开启。开启这些依赖进一步暴露 `host/gatt.c` 中缓存路径的multi-controller参数遗漏（hdev未传入、store函数签名不一致、delayable work container类型错误）。本次未伪造宏，也未声称server/sink已经完成；失败候选在 `out/esp32s31-ble-audio-shell` 保留，后续应修复该真实GATT路径并补角色。

以上证明应用入口已编译链接，不能替代controller初始化、对端CIS/BIG、BAP发现配置及音频互通实测。
