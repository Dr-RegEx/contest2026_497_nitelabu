# 板载 LED 演示

ESP32-S31 的板载 LED 演示应用源码。

构建选项见本目录 Kconfig 与构建文件，通过 openvela 板级配置集成；不能作为独立桌面程序运行。实测状态查阅 backups 的对应证据，不以源码存在认定已通过。

## 内容入口

- [CMakeLists.txt](CMakeLists.txt)
- [Kconfig](Kconfig)
- [Make.defs](Make.defs)
- [Makefile](Makefile)
- [s31led_main.c](s31led_main.c)
- [test_app.py](test_app.py)
