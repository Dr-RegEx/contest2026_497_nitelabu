# Mesh 模型入口与 SAR Configuration 接入

日期：2026-09-19。范围仅比赛清单第四部分 Mesh；不刷板、不使用串口或外设，不计空口测试通过。

## 确定阻断

锁定 ZBlue 源码已实现 SAR Configuration、Remote Provisioning、Private Beacon、Opcode Aggregator、Large Composition Data 等模型，但存在源码不等于当前镜像接入。

实际 NuttX 端口的 `___in_section` 是空宏，根命令通过 `port/sections/defines.c` 显式列表提供；Mesh 的 `SHELL_SUBCMD_SET_CREATE(model_cmds, (mesh, models))` 仍依赖 Zephyr 排序链接集合，其占位结构为零，`mesh models` 无法枚举 cfg/health 等模型。之前的递归解析修复只解决静态命令树，无法修复该缺失集合。

## 本次代码修改

- 端口 `SHELL_SUBCMD_COND_ADD` 导出的模型描述符改为可被外部注册表引用。
- `mesh/shell/nuttx_models.h` 按现有配置列出已编入的模型；`mesh init` 将描述符复制到有结束标记的静态命令表。这与 NuttX 根命令的显式注册机制一致，不需要新造协议处理函数。
- 注册表支持现有 cfg、health、SAR、RPR、LCD、Opcode Aggregator、Private Beacon 及其他已配置模型；未打开的选项不引入链接依赖。
- 隔离 `xts-flat-ble-mesh-init` profile 接入 SAR Configuration Server/Client 和原有 SAR shell。原 standalone composition 已含对应条件模型，因此启用后使用真实协议实现与真实 model composition。

## 验证

- 冻结旧配置下 Mesh shell/cfg/health 三个交叉编译对象成功；cfg 模型描述符为全局只读符号。
- `test-mesh-model-registry.py` 提取实际注册表代码，以合成 cfg/health/SAR 回调进行主机回归；启用 ASan/UBSan 与链接垃圾回收，验证三个模型可枚举、可调用、结束标记有效，结果见 `mesh-model-registry-test.log`。
- 全新目录 `openvela-dev/out/esp32s31-xts-flat-ble-mesh-sar-models-20260919` 完整构建 1826 步成功，进程退出 0，source 配置恢复。脚本 `build-mesh-sar-models.sh`，日志 `logs/build-mesh-sar-models.log`，回执 `build-mesh-sar-models.sha256`。
- 镜像 1402680 字节；SHA-256 `ebe19fa441f1bd1c0a167c6fb8f6c28908d0b40360289f7d2ffd57c85a507737`。
- 最终 `.config` 确认 `BT_MESH_SAR_CFG_SRV`、`BT_MESH_SAR_CFG_CLI`、`BT_MESH_SHELL_SAR_CFG_CLI` 为 y。`mesh-sar-models-symbols.log` 记录最终 ELF 中 cfg/health/SAR 三个命令描述符、SAR client transmitter/receiver get/set、SAR server opcode table/callback 等实际符号，证明没有再次被链接垃圾回收。
- 严格 callback 类型对象重编译、`git diff --check` 均通过。此前冻结镜像保留未覆盖。

## 边界

SAR 是本次具体接入的 Mesh 1.1 相关协议能力，不代表 Mesh 1.1 所有可选能力已启用或认证通过。配置数据、分段重传行为和模型消息互通仍需板上及第二节点验证。未来进入 `mesh` 后先 `mesh init`，再使用 `mesh models sar tx-get` 等现有命令；当前没有执行这些空口命令。
