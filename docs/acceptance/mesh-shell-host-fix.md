# Mesh 命令入口接入（仅主机，2026-09-19）

在 `mesh-host-init-fix.md` 的初始化修复基础上，继续修复 standalone 入口初始化后永久等待、无法输入实际 Mesh 命令的问题。

## 修改

- 从现有 NuttX ZBlue shell 提取 `zblue_shell_run()`；原 `zblue` 应用继续先初始化再调用它，`mesh` 应用完成异步蓝牙及 Mesh 初始化后复用同一交互解析器，不另造协议命令。
- 异步 callback 通过 semaphore 通知入口成功或失败；失败不进入 shell。成功后退出 shell 再进入 `mesh` 时复用已初始化的 Mesh，避免再次初始化。
- shell 原解析器只解析两级，`mesh prov pb-adv on`、`mesh models ...` 等嵌套命令无法到达真实处理函数；本次支持逐级静态命令分派，保留叶子参数转交和必需参数检查。
- shell context 改为静态存储，避免 Mesh 保存的 shell 指针随退出成为悬空栈指针；限制参数数量，避免原 32 项 argv 数组越界。

## 验证

- 新目录完整构建成功：`openvela-dev/out/esp32s31-xts-flat-ble-mesh-shell-20260919`，1822 步，退出码 0；日志 `logs/build-mesh-shell.log`。
- 镜像 1379904 字节，SHA-256 `4757d5d28cf9f609e0180871cd25bf2eea50eca76fa2c3e409a0046dc44d3db6`；回执 `build-mesh-shell.sha256`。
- `mesh-shell-entry-disassembly.log` 证明最终 ELF 的 `mesh_main` 包含初始化、等待 callback 和 `zblue_shell_run` 路径。System.map 含真实 `cmd_pb_adv`、`cmd_provision_local`、`cmd_net_send`。
- 主机回归脚本 `test-mesh-shell-dispatch.py` 提取当前实际分派函数，以合成命令表验证三级命令、叶子参数转交、缺参数拒绝、未知命令拒绝、两级 init、group help；AddressSanitizer/UBSan 运行通过，日志 `mesh-shell-dispatch-test.log`。这是解析器测试，不是 Mesh 空口测试。
- 严格 callback 类型交叉编译及 `git diff --check` 通过。未覆盖旧冻结镜像。

## 后续板上操作与边界

未来允许刷板时，在 NSH 输入 `mesh`，初始化成功进入 `zblue>` 后先输入 `mesh init`，再用完整命令 `mesh prov pb-adv on` 或 `mesh prov pb-gatt on`；模型命令由 `mesh models` 下现有命令表提供。`q` 返回 NSH，不能视为关闭蓝牙或取消配网。当前独立镜像应使用 `mesh` 为初始化入口，避免同时运行另一个 `zblue` 或 Bluetooth service 实例重复初始化同一协议栈。

本次完全未访问串口、未刷板；尚不能宣布 -19 已实板消除或 Mesh 配网/模型通信通过。需要第二节点的互通仍待设备。清单通过项数不因此增加。
