# LE Audio server/sink 与 LC3 入口：主机适配结果

日期：2026-09-19。仅工程内代码和主机编译验证，未刷写、未连串口、未增加实物通过项。

## BAP server/sink 必要修复

`xts-flat-ble-audio-shell` 候选补齐GATT dynamic DB/caching、ASCS、PACS、scan delegator和2个ATT Prepare缓冲。最终配置已实际开启BAP unicast server/client、broadcast sink/source，不再仅有defconfig中的无效请求。

- `host/gatt.c`：缓存查找、空位分配、数据库hash生成/保存等传递实际 `conn->hdev`/`hdev`，hash遍历调用对应controller的 `_mc` 接口；修复delayable work容器类型和重复 `gatt_ctx` 解引用。没有把controller硬编码为0。
- PACS和scan delegator安全变化回调补 `conn->hdev` 给bond查询，适配当前实际函数签名。
- ASCS/BASS服务是动态注册：BAP shell init → `bt_bap_unicast_server_register` → `bt_ascs_register` → `bt_gatt_service_register`；scan delegator shell注册 → `bass_register`。没有往静态服务表重复插入动态服务。
- 修复显式section表遗漏：ASCS和scan delegator connection callback用独立名字加入表；`broadcast_sink_init` 从错误ASSISTANT条件改为SINK条件，保证真实初始化能够运行。
- shell对unicast server和其callback注册失败立即返回真实错误，不继续报告初始化成功。

验证：`python3 tools/tests/gatt-cache-mc/run.py` 编译生产 `find_cf_cfg` 函数，两个controller拥有相同peer时分别命中各自缓存；identity不匹配不命中；空位查找不能借用另一个controller槽。UBSan fail-fast回归通过，日志 `ble-gatt-cache-mc-host.log`。

server/sink阶段独立完整构建 `build-ble-audio-roles.sh` 成功，固件 `out/esp32s31-ble-audio-roles-2/nuttx.bin` SHA256 `24f0d75cd5c5c6f6bb55c43848b31b029d0ae973c7f0ee07bfbe530b5374f0eb`。

## 实际 LC3 接入

项目codec包名为 `CONFIG_LIB_LC3`，ZBlue原入口判断 `CONFIG_LIBLC3`。增加仅随真实codec启用的Kconfig兼容映射，并在CMake链接既有 `liblc3` target及其公开头文件。候选开启codec，ISO TX缓冲数为4，满足实际LC3 sine发送入口首次预填2帧的要求；未引入外部下载或伪造编码器。

最终ELF含实际 `lc3_encode`、`lc3_decode`、decoder线程、`cmd_start_sine`、`cmd_stop_sine`、独立 `sine_tx_pool`；注册入口复用现有BAP命令。父代理已独立完成真实LC3主机100帧编解码及ASan/UBSan fail-fast回归，并修复codec既有signed-shift UB，参见 `le-audio-lc3-host-result.md`。

最终完整构建：

- 脚本：`build-ble-audio-lc3.sh`
- 日志：`build-ble-audio-lc3.log`，完整编译链接退出0，保留依赖原有警告。
- 候选固件：`openvela-dev/out/esp32s31-ble-audio-lc3/nuttx.bin`
- SHA256：`c3c6bd3dd2a1204968092fcd82368ce1029fa1f15687628d88ac4ba547f59985`
- 符号记录：`ble-audio-lc3-symbols.log`；哈希：`ble-audio-lc3.sha256`。
- 构建结束源树 `.config` 和 `include/nuttx/config.h` 已恢复，旧候选镜像未覆盖。

## 验证边界

当前证明必要协议路径、四种BAP角色和实际LC3入口可以编译链接，以及主机codec/缓存隔离回归。尚未证明板上controller初始化、CIS/BIG建立、对端发现/QoS、无线音频或扬声器输出。后续必须实际对端互通，不能将 `bap send` 测试字节或host codec回归算成LE Audio实物通过。

候选未启用 `BT_SETTINGS`，本轮没有验证持久化bond/cache加载路径；这不等同于证明整个GATT所有可选配置都已完成。
