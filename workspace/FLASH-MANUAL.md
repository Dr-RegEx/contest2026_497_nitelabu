# Korvo 固件部署与恢复

当前默认 Korvo 演示使用 `korvo-factory-vision`，按[组合固件说明](FACTORY-VISION.md)配对烧写内核与 AppFS。原版独立数字识别仍可按[专用烧录说明](../board/ESP32-S31%20Function-CoreBoard-1%20%2B%20ESP32-S31%20Korvo-1/firmware/korvo-digit-stable/README.md)部署。下文的板卡确认、备份和恢复方法通用；Factory 写入与数据区命令仅适用于独立 `korvo-openvela-factory`，不要直接用于组合配置。

本页用于 `korvo-openvela-factory` 的 16 MiB Korvo 板。该配置采用 simple boot：固件从 `0x2000` 开始，不需要外部 AppFS；应用数据位于 `0xe00000`，长度 `0x100000`。其他配置按各自镜像清单部署，Kernel 配置的内核与 AppFS 必须来自同一次构建。

新构建使用 `FIRMWARE=/path/to/new-factory-output/build`。构建入口同时生成镜像清单，先检查固件与清单：

```sh
python3 "$CONTEST_REPO/workspace/verify-artifacts.py" --directory "$FIRMWARE"
```

`$FIRMWARE` 应包含 `manifest.json`、`nuttx.bin`、`nuttx` 和 `.config`。工具只核对文件身份与布局，不连接板卡。新构建的实板状态见 [配置登记表](profiles.json)。

## 第一次安装

使用支持 ESP32-S31 的 esptool，连接板上的 USB-UART 接口。先关闭其他串口程序，将以下端口与目录改为本机实际值。

```sh
DEPS="$CHECKOUT/.s31-deps"  # 若安装时指定了 --deps，请改为同一个目录
ESPTOOL="$DEPS/venv/bin/esptool"
PORT=/dev/ttyUSB0
BACKUP_DIR=/path/to/new-korvo-backup
mkdir "$BACKUP_DIR"
"$ESPTOOL" --chip esp32s31 --port "$PORT" chip-id
"$ESPTOOL" --chip esp32s31 --port "$PORT" flash-id
"$ESPTOOL" --chip esp32s31 --port "$PORT" get-security-info
```

确认选中的是 Korvo，Flash 为 16 MiB。本开发配置适用于未启用安全启动和 Flash 加密的开发板；启用这些功能的板卡使用其签名和加密部署流程。

首次写入前保存完整原固件，两次读取一致后保留备份：

```sh
"$ESPTOOL" --chip esp32s31 --port "$PORT" read-flash 0 0x1000000 "$BACKUP_DIR/original-a.bin"
"$ESPTOOL" --chip esp32s31 --port "$PORT" read-flash 0 0x1000000 "$BACKUP_DIR/original-b.bin"
cmp "$BACKUP_DIR/original-a.bin" "$BACKUP_DIR/original-b.bin"
sha256sum "$BACKUP_DIR/original-a.bin" "$BACKUP_DIR/original-b.bin" > "$BACKUP_DIR/SHA256SUMS"
```

每条命令成功后再执行下一条。写入并校验镜像，保留镜像头中的 Flash 参数：

```sh
"$ESPTOOL" --chip esp32s31 --port "$PORT" write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep 0x2000 "$FIRMWARE/nuttx.bin"
"$ESPTOOL" --chip esp32s31 --port "$PORT" verify-flash \
  --flash-mode keep --flash-freq keep --flash-size keep 0x2000 "$FIRMWARE/nuttx.bin"
```

首次从其他系统切换到此配置时，应用数据区可能包含不兼容数据。完整备份已保存且确定不需要保留该区域后，可仅初始化 HMI 数据区；这会清除其设置和应用状态：

```sh
"$ESPTOOL" --chip esp32s31 --port "$PORT" erase-region 0xe00000 0x100000
"$ESPTOOL" --chip esp32s31 --port "$PORT" --after hard-reset run
```

后续升级同一配置通常保留应用数据。部署成功后可将新固件留在板上使用。串口参数为 115200，NSH 用于开发调试；最终演示界面的操作及验收范围见 [功能验证](../docs/validation/README.md)。

## 恢复原固件

需要回退时，使用这块板自身的完整备份。核对备份哈希后恢复并校验：

```sh
sha256sum -c "$BACKUP_DIR/SHA256SUMS"
"$ESPTOOL" --chip esp32s31 --port "$PORT" write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep 0 "$BACKUP_DIR/original-a.bin"
"$ESPTOOL" --chip esp32s31 --port "$PORT" verify-flash \
  --flash-mode keep --flash-freq keep --flash-size keep 0 "$BACKUP_DIR/original-a.bin"
"$ESPTOOL" --chip esp32s31 --port "$PORT" --after hard-reset run
```

保留本次固件哈希、部署记录和原始备份；完整 Flash 备份可能含设备设置与凭据，应留在自己的本地存储中。

## Function 最小系统、有线网和双核网络配置

同样先确认目标 UART、16 MiB Flash 并保存本板完整备份。`nsh` 与 `function-ethernet` 都只写该次构建的内核：

```sh
FIRMWARE=/path/to/new-build-output/build
"$ESPTOOL" --chip esp32s31 --port "$PORT" write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep 0x2000 "$FIRMWARE/nuttx.bin"
"$ESPTOOL" --chip esp32s31 --port "$PORT" verify-flash \
  --flash-mode keep --flash-freq keep --flash-size keep 0x2000 "$FIRMWARE/nuttx.bin"
```

复位后用 115200 串口观察 `nsh>`，运行 `uname -a`、`help`。有线网配置连接 Function 板载 RJ45 后，可用 `ifconfig eth0` 查看接口；IP 地址应按自己的网络设置。

`demo-rmt-netapps-competition` 必须同时写入同一输出目录中的内核和 AppFS。该配置将只读应用种子放在 `0x200000`，预留 `0x300000` 字节；可写应用存储从 `0x500000` 开始。先核对 `receipt.json` 中这两个文件的校验值与本次 `.config`，不要混用其他配置的分区布局。

```sh
FIRMWARE=/path/to/new-netapps-output/build
"$ESPTOOL" --chip esp32s31 --port "$PORT" write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  0x2000 "$FIRMWARE/nuttx.bin" 0x200000 "$FIRMWARE/appfs.img"
"$ESPTOOL" --chip esp32s31 --port "$PORT" verify-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  0x2000 "$FIRMWARE/nuttx.bin" 0x200000 "$FIRMWARE/appfs.img"
```

复位后检查 NSH、`ls /system/bin` 和 `hello`。本次公开依赖重建验证包含 kernel/AppFS 配对生成；不把构建成功扩大为本轮无线连接或外设测试通过。
