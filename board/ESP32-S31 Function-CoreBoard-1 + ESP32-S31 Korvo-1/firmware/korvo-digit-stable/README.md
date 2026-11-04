# Korvo 原版独立数字识别演示

本目录保留已实板验收的原版双核数字识别固件，供独立演示和回退使用。2026-11-04 已再次核验板上固件身份并确认双核、摄像头、显示及推理持续运行。当前默认演示为 [Factory 与原版识别组合配置](../../../../workspace/FACTORY-VISION.md)，本目录仍保留历史独立镜像。

上电自动显示摄像头画面。将白底深色的单个数字放入绿色框，保持居中、光线均匀，观察右侧识别结果与 28×28 预处理图。原人工验收确认画面及数字识别正常；不承诺任意字体和光照的准确率。

本固件使用 CPU0 摄像/显示与 CPU1 PIE/SIMD 推理，VGA 640×480、原始 200×200 识别区域。此独立固件不包含 Factory、录像/SD 或无线功能。需要 Factory 按钮进入并返回原版识别时，使用上述组合配置及其配对内核/AppFS。

## 烧录

仅适用于已确认的 ESP32-S31 Korvo-1、16 MiB Flash、OV3660 及 SUB3 显示屏。通过 USB-UART 连接，先关闭串口程序并保存本板原固件备份。使用支持 ESP32-S31 的 esptool，以下命令在本目录执行：

```sh
sha256sum -c SHA256SUMS
DEPS="$CHECKOUT/.s31-deps"  # 或依赖安装时指定的 --deps 目录
DIGIT_PORT=/dev/ttyUSB0
"$DEPS/venv/bin/python3" -m esptool --chip esp32s31 --port "$DIGIT_PORT" --baud 460800 write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m 0x2000 nuttx.bin
"$DEPS/venv/bin/python3" -m esptool --chip esp32s31 --port "$DIGIT_PORT" --baud 460800 verify-flash 0x2000 nuttx.bin
```

每条命令成功后再执行下一条。此固件无额外 AppFS；串口为 115200。不要将完整 Flash 备份公开提交。

## 源码与结果范围

`reference/` 是随稳定固件保存的三份源文件，`.config` 与 `defconfig` 保留原配置。完整源码恢复和构建入口位于 [workspace/digit-demo](../../../../workspace/digit-demo/README.md)，使用公开平台快照与原版覆盖层构建，不依赖开发者电脑上的私有源码。重建镜像已实测双核启动、100/100 自检及摄像/显示/推理持续运行，配置项与本目录原配置一致。历史固件继续保留；新构建含不同链接和构建元数据，不承诺逐字节一致，人工画面验收结果不自动迁移。

原主机计时记录：50.136 秒内 480 次确认显示交接，约 9.574 次/秒；不是光学帧率或识别准确率。CPU1 固定自检 100/100、SIMD parity_errors=0。逐文件校验见 `SHA256SUMS`，许可证见 `NOTICE.md` 和 `licenses/`。
