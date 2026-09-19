# Mesh 基本互通前的持久化与模型路径边界核对

2026-09-19：只读核查，未发现本轮可确认的新增移植代码缺陷；不修改配置、模型或主清单，不重跑已通过主机回归，不刷板/串口。

## 持久化现状

冻结完整 Mesh profile `openvela-dev/out/esp32s31-xts-flat-ble-mesh-sar-models-20260919/.config`：

- 第2628行：`# CONFIG_SETTINGS is not set`。
- 第2386行：`CONFIG_BT_MESH_RPL_STORAGE_MODE_SETTINGS=y` 仅指定 RPL 储存模式，不意味着 SETTINGS 后端实际编入。
- 第2483/2484行的 STORE_TIMEOUT=2、SEQ_STORE_RATE=128 同样不能当作已持久化证据。

实际 standalone 入口 `external/zblue/zblue/tests/bluetooth/mesh_shell/src/main.c`：bt_ready先真实bt_mesh_init，再仅在 CONFIG_SETTINGS 开启时执行 settings_load；随后按 bt_mesh_is_provisioned 输出恢复/未配网状态。本profile未加载持久化配置，重启后回到未配网状态，需要重新 provision，不能声称密钥、IV/SEQ/RPL跨重启恢复已完成。

没有据此断言存在已确认的SEQ重用漏洞：没有外设实测，也没有自动恢复同一旧网络密钥的路径证据。shell本地默认测试密钥的重新provision属于显式测试命令，不等于固件重启自动恢复。

`port/sections/defines.c:605` 起的NuttX显式settings注册表已有Mesh net/iv/seq/app/subnet/rpl/model handlers并按配置门控；本轮没有发现上轮命令注册那种确切遗漏。启用并验证持久化后端属于另一个配置/集成目标，依指令不擅自扩展。

## 当前真实模型接口

- shell/shell.c:974 的 cmd_provision_local 调用 bt_mesh_provision；PB-ADV/PB-GATT使用已有真实provision接口。
- shell/cfg.c 调用真实 bt_mesh_cfg_cli_*，shell/sar.c 调用真实 bt_mesh_sar_cfg_cli_transmitter/receiver get/set。
- 已完成的显式模型注册、SAR composition、shell命令可达性保留，本轮没有替换成模拟响应，也不重复计数。
- host/hci_core.c:4522 的 bt_enable_mc 在重复enable时会调用ready callback返回0，因此当前入口没有由“已启用”导致必然永久等待的新缺陷证据。

仍需对端闭合：首次provision成功、模型绑定/目标设置、cfg/health/SAR消息收发及实际响应/错误/超时。重启后需重新provision；持久化与跨重启恢复当前未启用、未验证。以上均不新增实物PASS。
