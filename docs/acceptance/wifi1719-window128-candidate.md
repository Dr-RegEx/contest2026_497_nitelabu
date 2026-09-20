# Wi‑Fi 性能候选 1719（低 RAM 版本）

更新：2026-09-18。

## 目的

1713 将 IOB 池提高到 192 个，虽然保留了 64 KiB TCP/UDP 窗口，但会明显
增加内部 RAM 占用。1719 保留窗口扩展和 SACK/乱序参数，只把 IOB 池降到
128，作为更安全的 5.1.22～5.1.25 上板候选。

## 离线验证

独立配置：
`openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/demo-rmt-bttool-coex-pie-netdiag-sack-window128/defconfig`

构建脚本：`build1719-wifi-window128.sh`。

构建成功，生成 `nuttx.bin`、`appfs.img`、iperf2、curl、scp、bttool、s31demo
和 s31simd；收据：`build1719-wifi-window128.sha256`。

2026-09-18 复核收据：对 `nuttx.bin` 和 `appfs.img` 执行
`sha256sum -c build1719-wifi-window128.sha256`，两项均为 `OK`。

最终配置确认：

- TCP window scale factor = 3；
- 收发缓冲及乱序缓存 = 65536；
- `CONFIG_IOB_NBUFFERS=128`；
- `.dram0.bss` = 119236 字节，较 1713 的 145604 字节减少 26368 字节；
- `g_iob_buffer` = 0xCE03，约 52.5 KiB，较 1713 约少 26 KiB。

该候选只在独立配置和离线构建中存在，未修改正式 SACK profile，也没有开发板
吞吐数据。它不能替代 5.1.22～5.1.25 的原始记录；需在同一 AP、同一主机和
原始 300 秒命令下实测后，才可判断性能是否改善。
