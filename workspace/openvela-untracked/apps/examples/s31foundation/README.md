# S31 基础接口集成检查

`s31foundation` 在实际目标库上执行一条有限链路：NuttX跳转上下文哨兵，JS Promise及Feature JSON桥接/libuv定时器，Yoga布局，protobuf序列化经过LittleFS写入/fsync/关闭/回读/解码/清理，合成PNG像素解码，ROMFS字体加载和字形栅格化。每次运行输出6项结果、总体结果及前后堆用量。E154已在Korvo原生HMI同镜像完成6轮实测，不是Quickapp运行时、完整Feature设备模块、吞吐或长稳验收。

仅创建 `/data/data/foundation-check.bin`，已有文件则拒绝覆盖；只清理本次创建的合成文件。PNG是生成的两个像素，不采集摄像头内容。FreeType直接读取 `/etc/fonts/Montserrat-Regular.ttf`；字体来自本地LVGL测试资产，未修改，197624字节，内嵌许可及随附OFL保留。该字体不代表完整中文/emoji覆盖，也未代替UIKit/UI线程渲染集成。

使用 `korvo-openvela-quickapp-check` CMake配置，由板级ROMFS规则打包字体与许可。Make入口提供程序编译规则，但本新配置尚未验证Make的资源打包和全量构建。Flash恢复必须覆盖固件与整个1MiB持久数据分区。详细证据由 `backups/quickapp-foundation-api-integration/` 维护，平台状态只更新唯一能力矩阵。
