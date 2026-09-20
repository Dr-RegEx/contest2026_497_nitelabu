# BLE Mesh / LE Audio 应用入口离线审计（1764）

日期：2026-09-19  
范围：只检查锁定 openvela profile 与随附 ZBlue/NimBLE 源码的应用入口；不刷写、不打开串口、不连接第二节点或音频对端。

## Mesh 1.1

`xts-flat-ble-mesh-init-1745` 已启用 `CONFIG_BT_MESH`、PB-ADV、PB-GATT、Provisionee、Relay 和 GATT Proxy，候选镜像的 Mesh 对象已链接。ZBlue 标准示例入口为 `bt_enable(bt_ready)` 后调用 `bt_mesh_init(&prov, &comp)`。

当前头文件 `include/zephyr/bluetooth/bluetooth.h` 将 `bt_ready_cb_t` 定义为带 `uint8_t dev_id, int err` 两个参数的回调；示例 `samples/bluetooth/mesh/src/main.c` 和 `tests/bluetooth/mesh_shell/src/main.c` 仍使用单参数 `static void bt_ready(int err)`。直接复用 `mesh_shell` 入口时，当前生成的兼容头还缺少 `BT_MESH_KEY_UNUSED_ELT_` 展开和 `CONFIG_BT_COMPANY_ID` 等配置上下文。用锁定 S31 交叉编译命令加 `-Werror=incompatible-pointer-types` 做了隔离检查，入口不能作为当前板级 profile 的可直接集成证明（编译以非零返回结束）。

本轮不新增空壳 `bt_enable`/`bt_mesh_init` 包装，不改默认配置，也不把 Mesh 子系统已链接误记为应用初始化或双节点通过。后续有板卡条件时应先修正 callback/config 上下文，再记录 `bt_enable`、`bt_mesh_init`、PB-ADV/PB-GATT 启动日志；provision/model 收发还需要第二节点。

## LE Audio / ISO / BAP

`xts-flat-ble-audio-mesh-1727` 已静态链接 ISO central/peripheral/broadcast 和 BAP client/source 目标，但 `.config` 中的 ISO 开关只证明 host 编译路径。当前没有 S31 controller 的 CIS/BIG 建立证据，也没有手机、耳机或第二块板作为 BAP/ISO 对端。应用层没有可在无对端条件下证明音频流建立的离线入口，因此不添加伪造的 ISO/BAP 成功路径。

## 结果

- Mesh：候选镜像/协议栈静态存在；应用入口兼容性审计未通过，保持“待兼容修正及双节点实测”。
- LE Audio：ISO/BAP 静态构建保持有效；controller/对端互通缺失，保持“待对端互通”。
- xTS 通过数：不增加。
- 默认 profile：未修改、未启用新的 Mesh/Audio 选项。
