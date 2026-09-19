# BLE Mesh 1.1 隔离应用入口构建证据（1766）

日期：2026-09-19  
范围：仅验证用户自添加第 10 项的离线源码兼容性与 S31 镜像链接；不刷板、不打开串口、不连接第二节点。

## 修正内容

- 将 `tests/bluetooth/mesh_shell/src/main.c` 的 `bt_ready` 改为当前 ZBlue `bt_ready_cb_t` 的双参数签名，并保留真实 `bt_enable()` -> `bt_mesh_init()` 调用链。
- 在 `CMakeLists.default.txt` 中仅在 `CONFIG_BT_MESH_SHELL` 下注册 `mesh` 应用；修正应用目标为 `apps_zblue`。
- 新增隔离板级 profile `xts-flat-ble-mesh-init`，启用 Mesh、PB-ADV、PB-GATT、Provisionee、Relay、GATT Proxy、Config/Health Client 和 `CONFIG_BT_COMPANY_ID`。默认生产 profile 未改。
- 为 ZBlue 时间转换头增加 `CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC` 到 `CONFIG_ZBLUE_SYS_CLOCK_HW_CYCLES_PER_SEC` 的兼容回退。

## 构建结果

复现命令：

```text
bash backups/2026-09-10-scan-stress/build1765-ble-mesh-entry.sh \
  /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build1766-ble-mesh-entry.sha256
```

- S31 Ninja 构建：`1800/1800`，返回码 `0`。
- 镜像：`openvela-dev/out/esp32s31-xts-flat-ble-mesh-entry-1765/nuttx.bin`，`1,379,600` 字节。
- SHA-256：`32fcc7e95949a529cbe0defaf8effdfd64e62d84f667d8ec4c43ee37523a5209`。
- `System.map` 包含 `mesh_main`、`bt_enable_mc`、`bt_mesh_init`。
- 从 `compile_commands.json` 提取入口命令，追加 `-Wincompatible-pointer-types -Werror=incompatible-pointer-types` 后重编译成功，输出 `/tmp/s31-mesh-entry-strict.o`（7964 字节）。
- `git diff --check` 通过。

## 结论与边界

Mesh 应用入口已完成离线兼容修正并进入隔离镜像。该证据只证明配置、入口编译和最终链接，不证明板上 Bluetooth 初始化、PB-ADV/PB-GATT 广播、provision、model 收发或 BLE Mesh 1.1 双节点互通；因此不增加 xTS 通过数，仍需实物验证后才能闭合。
