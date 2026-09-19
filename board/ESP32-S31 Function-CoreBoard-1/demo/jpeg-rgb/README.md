# JPEG 编码/解码与 RGB888/RGB565

## 已通过范围

- 用户自添加第3项：🟢 板级自测通过；`jpegtest`在1725 JPEG/RGB镜像实板执行，JPEG SOI及编码输出通过；证据：[media1725](../../../../docs/acceptance/media1725-jpeg-rgb-board-build.md)、`jpeg1728-board-test/uart.log`
- 用户自添加第4项：🟢 板级自测通过；`jpegtest`实板解码2×2 RGB888并完成输出尺寸校验；证据：[media1725](../../../../docs/acceptance/media1725-jpeg-rgb-board-build.md)、`jpeg1728-board-test/uart.log`
- 用户自添加第6项：🟢 板级自测通过；`jpegtest`实板输出2×2 RGB888，逐像素参与JPEG往返；证据：[media1725](../../../../docs/acceptance/media1725-jpeg-rgb-board-build.md)、`jpeg1728-board-test/uart.log`
- 用户自添加第7项：🟢 板级自测通过；`jpegtest`实板执行RGB888→RGB565打包并校验8字节结果；证据：[media1725](../../../../docs/acceptance/media1725-jpeg-rgb-board-build.md)、`jpeg1728-board-test/uart.log`

NSH 执行 jpegtest，预期 `PASS JPEG bytes=713 decode=2x2 RGB888=12 bytes RGB565=8 bytes`。只验证固定 2×2 样本 JPEG 编解码和 RGB888/RGB565，不依赖摄像头，也不代表 Camera 实采通过。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/examples/jpegtest](../_sources/apps/examples/jpegtest)：原工程 `apps/examples/jpegtest`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[xts-flat-jpeg-rgb](../../configs/xts-flat-jpeg-rgb/defconfig)。
- 镜像类型：FLAT（仅 nuttx.bin）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-jpeg-rgb 8
```

产物位于 `out/esp32s31-xts-flat-jpeg-rgb/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

NSH 执行 jpegtest，预期 `PASS JPEG bytes=713 decode=2x2 RGB888=12 bytes RGB565=8 bytes`。只验证固定 2×2 样本 JPEG 编解码和 RGB888/RGB565，不依赖摄像头，也不代表 Camera 实采通过。

## 历史通过依据

- [ble1725-board-basic/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/ble1725-board-basic/result.json)
- [ble1725-static-build-audit.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/ble1725-static-build-audit.md)
- [media1725-jpeg-rgb-board-build.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/media1725-jpeg-rgb-board-build.md)
- [logs/ble1725-idf-export.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/ble1725-idf-export.log)
- [ble1725-board-basic/uart.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/ble1725-board-basic/uart.log)
- [ble1725-static-build-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/ble1725-static-build-result.json)
- [jpeg1728-board-test/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/jpeg1728-board-test/result.json)
- [ble1728-mesh-build.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/ble1728-mesh-build.md)
- [jpeg1728-board-test/uart.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/jpeg1728-board-test/uart.log)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
