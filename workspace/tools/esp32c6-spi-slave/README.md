# S31 / C6 通用 SPI 功能测试

S31 为硬件 SPI2 主机，C6 为硬件 SPI2 从机。2026-09-19 已上板通过：100kHz、1MHz，14帧主机发送均被C6正确接收，13帧延迟回传均与上一帧原文一致。

## 断电接线

两板 USB 都拔下再接线，完成后分别通过 USB 连接电脑。不要互连 5V 或 3V3。

|S31 丝印|C6 丝印|信号|
|---|---|---|
|45|4|SCLK|
|46|5|MOSI，S31 发往 C6|
|47|6|MISO，C6 发往 S31|
|48|7|CS，低电平有效|
|GND|GND|共地|

前两根和 GND 可以沿用 I2C 接线。C6 GPIO4/5 涉及启动采样；本夹具与此前 I2C 一样使用这些脚，上电时主机不应主动拉低从机启动脚。若不能正常启动，需要先确认启动日志，不能把启动失败当作 SPI 结果。

## 协议与验收

Mode 0、MSB first、每字节8位、每帧16字节。C6 第一帧返回 A0..AF，之后每帧返回上一次收到的16字节。主机两帧之间留出至少100ms，供 C6 打印日志并重新挂起接收。先测100kHz，再测1MHz，发送不同模式并比对上一帧原文，同时核对 C6 接收日志。

示例 S31 命令：

```text
spi exch -f100000 -x16 000102030405060708090a0b0c0d0e0f
spi exch -f100000 -x16 55555555555555555555555555555555
```

第二条应收到 `00 01 ... 0F`。接收到全零或仅命令成功不能判通过。

## 构建

C6：在本目录使用锁定的 `s31-reference/tmp/esp-idf-clean/export.sh` 环境执行 `idf.py build`；确认设备后 `idf.py -p /dev/ttyACM0 flash`。

S31：配置 `esp32s31-core-function-board:xts-flat-spi-c6`；实测固件 `openvela-dev/out/esp32s31-xts-flat-spi-c6/nuttx.bin`，FLAT、无需AppFS，偏移0x2000。当前硬件SPI驱动为CMake路径下的8位轮询实现，尚未验证DMA、其他字宽和其他SPI模式；不代表完整SPI性能或全部模式适配完成。

## 自动验证

在工程根目录执行（依赖 pyserial）：

```bash
python3 tools/esp32c6-spi-slave/verify_spi.py --master /dev/ttyUSB0 --slave /dev/ttyACM1 --log /tmp/spi-new.log
```

串口编号必须按实际连接确认；日志路径必须不存在。首次返回内容仅记录，后续13帧必须逐字节等于上一帧发送内容，且全部14帧都必须有C6串口接收证据。本次结果见 `../../backups/2026-09-10-scan-stress/spi-c6-result.md`。
