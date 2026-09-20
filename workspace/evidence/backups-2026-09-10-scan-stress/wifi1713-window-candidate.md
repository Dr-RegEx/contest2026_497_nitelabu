# Wi-Fi 性能候选 1713

更新：2026-09-18。

## 目的

5.1.22～5.1.25 的现有 300 秒记录均低于行业参考值。候选 1713 在原
`demo-rmt-bttool-coex-pie-netdiag-sack` 配置上增加 TCP 窗口扩展和更大的
网络缓冲，保留原始 iperf2 命令、300 秒工作量及 SACK/乱序恢复。

## 变更

独立配置：
`openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-rmt-bttool-coex-pie-netdiag-sack-window/defconfig`

启用或调整：

- `CONFIG_NET_TCP_WINDOW_SCALE=y`，因子 3（8 倍窗口比例）；
- `CONFIG_NET_RECV_BUFSIZE` / `CONFIG_NET_MAX_RECV_BUFSIZE` = 65536；
- `CONFIG_NET_SEND_BUFSIZE` / `CONFIG_NET_MAX_SEND_BUFSIZE` = 65536；
- `CONFIG_NET_TCP_OUT_OF_ORDER_BUFSIZE` = 65536；
- `CONFIG_IOB_NBUFFERS` = 192。

该配置未覆盖原始 SACK profile，也未修改正式网络镜像。增加 IOB 数量会占用
更多静态内存，所以先保持为候选配置，不能直接替换正式镜像。

## 验证

构建脚本：`build1713-wifi-window.sh`。

构建结果：成功生成 `nuttx.bin`、`appfs.img`、iperf2、curl、scp、bttool、
s31demo 和 s31simd；镜像大小分别为 1,606,220 和 3,089,408 字节。构建收据：
`build1713-wifi-window.sha256`。

## 离线复核

1713 的生成配置 `out/esp32s31-spawn-kernel1713/defconfig` 和
`include/nuttx/config.h` 均确认最终值已经生效：TCP window scale factor 为
3，收发缓冲及乱序缓存均为 65536，`CONFIG_IOB_NBUFFERS=192`。该目录的
`config_tree.json` 同时保留了候选覆盖项，说明不是只写在 defconfig 而未进入
实际配置。`#include` 是本工程 defconfig 预处理器采用的继承语法，正式 SACK
配置文件本身未被改动。

静态链接结果的 `.dram0.bss` 为 145604 字节，芯片内部 RAM 配置为 524288
字节；IOB 池符号 `g_iob_buffer` 约 0x13503 字节，192 个 400 字节 IOB 相比
原 96 个会额外占用约 38.4 KiB（另有管理开销）。因此该候选存在明显的 RAM
占用代价，必须以启动日志和原始 300 秒吞吐实测确认稳定性，不能仅凭构建结果
替换正式镜像。

当前没有开发板人工操作条件，1713 尚未烧录，也没有新的 300 秒吞吐数据。
因此 5.1.22～5.1.25 的状态和原始证据不变；该候选只能在同一 AP、同一主机
工具和原始 300 秒命令下实测后，才能判断是否改善或替换记录。
