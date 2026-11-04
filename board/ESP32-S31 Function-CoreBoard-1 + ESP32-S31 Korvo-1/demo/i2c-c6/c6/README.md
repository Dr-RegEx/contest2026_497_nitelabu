# I2C-C6 配套 C6 工程

本目录保留历史 fixture 源码和验证脚本，来源为 `workspace/tools/esp32c6-i2c-slave`。S31 应用来源、许可和 canonical 配置见 [例程说明](../README.md)。历史结果与边界见 [验证索引](../../../../../docs/validation/README.md)，旧结果不作为新固件验收。

历史连线 S31→C6：45→4（SCL）、46→5（SDA）、GND→GND。断电接线，分别 USB 供电，不互连 5V/3V3。C6 工程使用 ESP-IDF `14f663f003eb8fd9a688c301a412a9540d29dacf`；在已准备好该版本环境的终端进入本目录，以 `idf.py build` 构建。设备操作时按实际板卡选择端口，S31 镜像使用其 canonical profile 及对应镜像布局。

原工程的协议与实现说明：


## 实现要点

C6 收到读请求后，通过事件队列唤醒任务调用 `i2c_slave_write`，提交响应并释放时钟延展。接收数据随事件复制，避免共享缓冲区被下一笔覆盖；检查发送长度与事件丢失。不可仅在启动时预填响应而忽略后续读请求。

