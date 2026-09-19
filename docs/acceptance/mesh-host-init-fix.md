# Mesh standalone 初始化修复（仅主机构建）

日期：2026-09-19。范围：比赛清单第四部分 BLE Mesh；本次不刷写、不占用串口、不增加实测通过数。

## 原因与代码修改

既有实板日志 `mesh-board-init-uart.log` 显示 `mesh` 直接返回 `Bluetooth init failed (err -19)`。

独立入口 `external/zblue/zblue/tests/bluetooth/mesh_shell/src/main.c` 缺少 NuttX 端口初始化。框架 `bt_sal_le_init()` 和 `zblue` shell 都先运行 `z_sys_init()`，而原 `mesh` 入口直接调用 `bt_enable()`。当前 H4 使用 `DEVICE_DT_DEFINE`；设备初始化状态必须由 init entries 设置，否则 `bt_enable_mc()` 的 `device_is_ready(hdev->hci)` 检查返回 `-ENODEV`。同一初始化过程还启动系统工作队列。

本次在 `__NuttX__` 条件下补齐 `z_sys_init()`，并在同步 enable 失败时返回失败码，避免进入永久等待并显示误导性的后续操作提示。此前已存在的双参数 ready callback 修正保留。未修改蓝牙版本标号或协议能力声明。

## 验证与复现

- `build-mesh-host-init.sh` 在全新目录 `openvela-dev/out/esp32s31-xts-flat-ble-mesh-host-init-20260919` 完整构建成功，1822 个构建步骤，退出码 0。
- 完整输出：`logs/build-mesh-host-init.log`；镜像回执：`build-mesh-host-init.sha256`。
- 镜像大小 1379612 字节，SHA-256 `de8139afa698b3e9dfdea1fce9a46ed5a24d0b480e6058351f4e038c0db53c8e`。
- 入口严格回调类型交叉编译通过；`git diff --check` 通过。
- `mesh-host-init-disassembly.log` 证明最终 ELF 中 `mesh_main` 先调用 `z_sys_init`，再调用 `bt_enable_mc`，不是被条件编译去掉的源码修改。
- 原冻结镜像 `esp32s31-xts-flat-ble-mesh-entry-1765/nuttx.bin` 未覆盖，SHA-256 仍为 `32fcc7e95949a529cbe0defaf8effdfd64e62d84f667d8ec4c43ee37523a5209`。

复现构建（传入尚不存在的绝对回执路径）：

```sh
bash backups/2026-09-10-scan-stress/build-mesh-host-init.sh /tmp/mesh-host-init-new.sha256
```

脚本临时保存并在退出时恢复 source `.config` 和 `include/nuttx/config.h`，使用现有锁定 IDF/HAL 和离线依赖，不下载或重建仓库。

## 剩余限制

这是由真实失败定位得到的代码修复和完整链接证据；尚未重新上板，不能声称 -19 已实板消除，更不能证明 Mesh 配网、模型消息或双节点互通。当前 standalone 入口仍是隔离启动入口，交互 shell 生命周期与反复启动需后续处理；本次不扩展为已经完成的 Mesh 1.1 全部功能。
