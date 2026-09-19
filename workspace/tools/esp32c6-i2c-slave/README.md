# S31 主机 / C6 从机：通用 I2C 回传测试

本工具验证 I2C 数据收发，不依赖传感器协议。C6 将最近一次收到的完整数据原样回传；主机每次读取的长度必须与最近写入长度一致。当前实测范围为 1、2 字节，100 kHz、400 kHz。不是任意长度流式协议。

## 接线

|S31 丝印|C6 丝印|用途|
|---|---|---|
|45|4|SCL|
|46|5|SDA|
|GND|GND|共地|

两块板分别通过 USB 接电脑供电；不互连 5V。地址为 7 位 `0x68`。使用现有接线及 C6 内部上拉。

## 构建及刷入

在工程根目录，先确认串口无其他进程占用。串口编号可能随连接变化；本次 S31 为 `/dev/ttyUSB0`，C6 为 `/dev/ttyACM0`。刷写和打开串口可能重启板卡。

```bash
source s31-reference/tmp/esp-idf-clean/export.sh
cd tools/esp32c6-i2c-slave
idf.py build
idf.py -p /dev/ttyACM0 -b 460800 flash
```

S31 使用独立配置 `esp32s31-core-function-board:xts-flat-i2c-c6`，本次已验证固件位于 `openvela-dev/out/esp32s31-xts-flat-i2c-c6-3/nuttx.bin`。此为 FLAT 专用测试镜像，无需 AppFS，并非所有功能的合并镜像。工程根目录执行：

```bash
esptool --chip esp32s31 --port /dev/ttyUSB0 --baud 460800 write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m 0x2000 openvela-dev/out/esp32s31-xts-flat-i2c-c6-3/nuttx.bin
python3 tools/esp32c6-i2c-slave/verify_echo.py --log /tmp/i2c-echo-new.log
```

脚本依赖 pyserial；日志路径必须尚不存在，避免覆盖历史证据。共 32 组写入/读取比对，两个频率、两种长度各 8 组。`-w16` 指两字节数据，不是总线地址宽度。

## 实现要点

C6 收到读请求后，通过事件队列唤醒任务调用 `i2c_slave_write`，提交响应并释放时钟延展。接收数据随事件复制，避免共享缓冲区被下一笔覆盖；检查发送长度与事件丢失。不可仅在启动时预填响应而忽略后续读请求。

实测记录与固件哈希见 `../../backups/2026-09-10-scan-stress/i2c-generic-echo-result.md`。
