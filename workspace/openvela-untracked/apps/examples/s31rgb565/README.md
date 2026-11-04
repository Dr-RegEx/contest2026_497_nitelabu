# RGB565 显示演示

ESP32-S31 的RGB565 显示演示应用源码。

构建选项见本目录 Kconfig 与构建文件，通过 openvela 板级配置集成；不能作为独立桌面程序运行。实测状态查阅 backups 的对应证据，不以源码存在认定已通过。

## 内容入口

- [CMakeLists.txt](CMakeLists.txt)
- [Kconfig](Kconfig)
- [Make.defs](Make.defs)
- [Makefile](Makefile)
- [patterns.c](patterns.c)
- [patterns.h](patterns.h)
- [s31rgb565_main.c](s31rgb565_main.c)
- [startup.c](startup.c)
