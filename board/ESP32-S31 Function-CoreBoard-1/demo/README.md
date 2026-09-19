# ESP32-S31 已通过测试例程

本目录整理当前验收清单的 **78 个绿色 xTS 条目**与 **6 个绿色用户新增能力条目**，共 82 个例程目录。I2C/SPI 分别建立独立例程；JPEG 编解码、RGB888/RGB565 合并到同一 `jpeg-rgb` 例程；BLE 开关在第三部分的重复行不重复收录。

每个例程包含 README、`src/` 实际源码、`config/` 完整继承配置、`evidence/` 历史证据和结构化索引；目录内均为普通文件。每份 README 直接包含环境准备、拉取工程、构建、刷写和运行步骤。C6 例程的 `c6/` 直接包含夹具工程与主机脚本。`source-sha256.json` 记录每个副本的原工程路径与哈希，`catalog.json` 覆盖全部收录条目。

## 使用方法

1. 打开所需例程目录，按其 README 内的完整命令准备工作区并构建。
2. 使用该例程给出的刷写与运行步骤验证，并将本次结果保存为新日志。
3. 源码、配置、原始用例及历史证据均保存在该例程内部，不需要追踪符号链接或跨目录寻找材料。

不同例程使用不同 profile；不能声称所有功能在一份镜像中同时通过。例程依赖完整 openvela 操作系统与工具链，README 已列出获取方式。修改 `src/` 后按例程中的同步命令更新工作区编译树。

## 验收口径

- 收录依据为清单绿色行，不把本次文档整理当作重测。各目录注明实板 PASS、用户确认或限定自测范围。
- RNG 为用户确认通过，原始未定义统计结果保留。
- I2C/SPI 为用户指定的通用通信，不替代传感器原始用例。
- nxlooper 仅命令路径无报错，扬声器听验仍待完成；录音条目包含后续用户试听确认。
- JPEG/RGB 为固定样本、SIMD 为计算和上下文验证、GATT 为列明读写范围。
- Camera、SDMMC、USB、BLE Mesh、LE Audio 等待实物项目，以及待验收的 Wi-Fi 速率和文件系统性能项目没有列入已通过目录。
- Flash、文件系统崩溃/掉电等例程会修改介质，需先核验专用测试分区并备份；长测、按键、真实断电和听音步骤均由执行测试的成员按 README 操作。本次整理不操作板卡。

## 例程索引

|目录|用例|当前构建配置|
|---|---|---|
|[common-1.1.1](common-1.1.1/README.md)|系统内存管理测试|`demo-rmt-xts-core`|
|[common-1.1.2](common-1.1.2/README.md)|系统调度测试|`demo-rmt-xts-core`|
|[common-1.1.3](common-1.1.3/README.md)|系统调用测试|`demo-rmt-xts-common`|
|[common-1.1.4](common-1.1.4/README.md)|Kernel-ostest测试|`demo-rmt-xts-ostest`|
|[common-1.1.5](common-1.1.5/README.md)|Kernel-getprime测试|`demo-rmt-xts-common`|
|[common-1.1.6](common-1.1.6/README.md)|Kernel-mm内存测试|`xts-flat-rtc`|
|[common-1.1.7](common-1.1.7/README.md)|Kernel-scanftest扫描测试|`demo-rmt-xts-common`|
|[common-1.1.8](common-1.1.8/README.md)|Kernel-C测试|`demo-rmt-xts-common`|
|[common-1.1.9](common-1.1.9/README.md)|Kernel-Cxx测试|`demo-rmt-xts-cxx`|
|[common-1.1.10](common-1.1.10/README.md)|Kernel-popen测试|`demo-rmt-xts-io`|
|[common-1.1.11](common-1.1.11/README.md)|Kernel-pipe测试|`demo-rmt-xts-io`|
|[common-1.1.12](common-1.1.12/README.md)|Kernel-md5值测试|`demo-rmt-xts-io`|
|[common-1.1.13](common-1.1.13/README.md)|Kernel-C++功能测试|`demo-rmt-xts-cxx`|
|[common-1.2.1](common-1.2.1/README.md)|Reboot启动异常测试|`demo-rmt-xts-reboot`|
|[common-1.2.2](common-1.2.2/README.md)|Cold boot启动异常测试|`demo-rmt-netapps-competition`|
|[common-1.2.3](common-1.2.3/README.md)|系统RAM资源占用统计|`demo-rmt-xts-io`|
|[common-1.2.4](common-1.2.4/README.md)|系统Flash资源占用统计|`demo-rmt-xts-io`|
|[common-1.3.1](common-1.3.1/README.md)|烧写测试|`demo-rmt-xts-core`|
|[common-1.3.2](common-1.3.2/README.md)|RAM读写测试|`demo-rmt-xts-memory`|
|[common-1.3.3](common-1.3.3/README.md)|RAM读写性能测试|`demo-rmt-xts-memory`|
|[common-1.3.4](common-1.3.4/README.md)|RAM随机读写测试|`xts-flat-rtc`|
|[common-1.3.5](common-1.3.5/README.md)|Flash功能测试|`xts-flat-flash`|
|[common-1.3.10](common-1.3.10/README.md)|Uart串口功能测试|`demo-rmt-xts-drivers`|
|[common-1.3.11](common-1.3.11/README.md)|Uart文件传输功能测试|`demo-rmt-xts-ymodem`|
|[common-1.3.12](common-1.3.12/README.md)|RTC时钟功能测试|`xts-flat-rtc`|
|[common-1.3.13](common-1.3.13/README.md)|Timer定时器功能测试|`demo-rmt-xts-drivers`|
|[common-1.3.14](common-1.3.14/README.md)|时间一致性测试|`demo-rmt-xts-standby`|
|[common-1.3.15](common-1.3.15/README.md)|Watchdog测试|`xts-flat-wdt`|
|[common-1.3.16](common-1.3.16/README.md)|RNG功能测试|`demo-rmt-xts-rng`|
|[common-1.3.17](common-1.3.17/README.md)|Crypto功能测试|`demo-rmt-xts-ecc-offline-log`|
|[common-2.1.3](common-2.1.3/README.md)|Cold Boot启动时间测试|`demo-rmt-netapps-competition`|
|[common-2.1.4](common-2.1.4/README.md)|Reboot启动时间测试|`demo-rmt-xts-reboot`|
|[common-3.1.1](common-3.1.1/README.md)|12h待机稳定性测试|`demo-rmt-xts-standby`|
|[category-4.1.2](category-4.1.2/README.md)|文件系统--romfs只读文件系统测试|`xts-flat-category-fs`|
|[category-4.1.3](category-4.1.3/README.md)|fatutf8文件系统编码能力测试|`xts-flat-category-fs`|
|[category-4.1.4](category-4.1.4/README.md)|文件系统异常场景--写入时掉电|`xts-flat-category-fs-sync`|
|[category-4.1.5](category-4.1.5/README.md)|文件系统异常场景--创建文件时掉电|`xts-flat-category-fs-sync`|
|[category-4.1.6](category-4.1.6/README.md)|文件系统异常场景--掉电重启时磁盘空间变化|`xts-flat-category-fs-sync`|
|[category-4.1.11](category-4.1.11/README.md)|KVDB测试|`xts-flat-category-kv`|
|[category-4.1.21](category-4.1.21/README.md)|WiFi--2.4G--Wapi连接为WPA2加密方式时加入2.4G网络|`demo-rmt-netapps-competition`|
|[category-4.1.25](category-4.1.25/README.md)|WiFi--2.4G--Wapi Sense获取链接2.4G AP的RSSI|`demo-rmt-netapps-competition`|
|[category-4.1.26](category-4.1.26/README.md)|WiFi--2.4G--Wapi Show获取2.4G AP的无线参数|`demo-rmt-netapps-competition`|
|[category-4.1.28](category-4.1.28/README.md)|WiFi--2.4G--wapi config保存正常|`demo-rmt-netapps-competition`|
|[category-4.1.29](category-4.1.29/README.md)|WiFi--2.4G--wapi config保存后设备reboot再reconnect|`demo-rmt-netapps-competition`|
|[category-4.1.30](category-4.1.30/README.md)|WiFi--2.4G--wapi reconncet|`demo-rmt-netapps-competition`|
|[category-4.1.31](category-4.1.31/README.md)|WiFi--2.4G--wapi disconnect后与外网不通|`demo-rmt-netapps-competition`|
|[category-4.1.57](category-4.1.57/README.md)|WiFi--wapi CountryCode的设置和获取|`demo-rmt-netapps-competition`|
|[category-4.1.113](category-4.1.113/README.md)|NetApp--curl http文件下载测试|`demo-rmt-netapps-competition`|
|[category-4.1.114](category-4.1.114/README.md)|NetApp--curl网页能力测试|`demo-rmt-netapps-competition`|
|[category-4.1.115](category-4.1.115/README.md)|NetApp--设备支持通过scp方式从其他终端获取文件到本地|`demo-rmt-netapps-competition`|
|[category-4.1.116](category-4.1.116/README.md)|NetApp--设备支持通过scp方式向其他终端传输本地文件|`demo-rmt-netapps-competition`|
|[category-4.1.117](category-4.1.117/README.md)|NetApp--设备支持ftpd|`demo-rmt-netapps-competition`|
|[category-4.1.127](category-4.1.127/README.md)|Audio--nxlooper回环功能测试|`xts-flat-audio-duplex`|
|[category-4.2.12](category-4.2.12/README.md)|Audio_DSP--Capture_cmoka_2channels|`xts-flat-audio`|
|[category-4.2.13](category-4.2.13/README.md)|Audio_DSP--Capture_cmoka_1channels|`xts-flat-audio-mono-args`|
|[category-5.1.1](category-5.1.1/README.md)|文件系统分区写满异常测试|`xts-flat-category-fs-name`|
|[category-5.1.2](category-5.1.2/README.md)|文件系统文件写入速度测试vela_fs_stress_write_speed|`xts-flat-category-fs-name`|
|[category-5.1.3](category-5.1.3/README.md)|文件系统创建和删除压力测试vela_fs_stress_loop_create_delete_file_test|`xts-flat-category-fs-name`|
|[category-5.1.4](category-5.1.4/README.md)|文件系统反复创建和删除文件vela_fs_stress_maximum_file_name_test|`xts-flat-category-fs-name`|
|[category-5.1.5](category-5.1.5/README.md)|文件系统多线程文件读写测试vela_fs_stress_multi_thread_file_operate_test|`xts-flat-category-fs-name`|
|[category-5.1.6](category-5.1.6/README.md)|文件系统反复读写测试vela_fs_stress_read_and_write_loops_test|`xts-flat-category-fs-name`|
|[category-5.1.8](category-5.1.8/README.md)|文件系统多线程写文件测试vela_fs_multi_thread_write_test|`xts-flat-category-fs-name`|
|[category-5.1.9](category-5.1.9/README.md)|文件系统多线程读文件vela_fs_multi_thread_read_test|`xts-flat-category-fs-name`|
|[category-5.1.10](category-5.1.10/README.md)|文件系统的多线程读写测试vela_fs_multi_thread_read_write_test|`xts-flat-category-fs-name`|
|[category-5.1.13](category-5.1.13/README.md)|裸设备读写性能测试|`xts-flat-flash-raw`|
|[category-5.1.15](category-5.1.15/README.md)|文件系统fstest执行1000次|`xts-flat-category-fs-name`|
|[category-5.1.16](category-5.1.16/README.md)|文件系统碎片读写测试|`xts-flat-category-fs-large`|
|[category-5.1.17](category-5.1.17/README.md)|文件系统多线程操作文件测试|`xts-flat-category-fs-large`|
|[category-5.1.18](category-5.1.18/README.md)|文件系统crash后crc校验 vela_fs_stability_test03|`xts-flat-category-fs-sync`|
|[category-5.1.19](category-5.1.19/README.md)|文件系统crash后文件完整性校验vela_fs_stability_test04 文件|`xts-flat-category-fs-sync`|
|[category-5.1.26](category-5.1.26/README.md)|WiFi--2.4G网络---设备连接2.4G后wapi反复扫描周边无线AP|`demo-rmt-netapps-competition`|
|[category-5.1.27](category-5.1.27/README.md)|WiFi--2.4G网络---设备连接2.4G后wapi扫描指定2.4G AP|`demo-rmt-netapps-competition`|
|[category-5.1.29](category-5.1.29/README.md)|WiFi--2.4G网络---wapi反复disconnect 2.4G网络|`demo-rmt-netapps-competition`|
|[category-5.1.41](category-5.1.41/README.md)|WiFi--反复ifup&ifdown|`demo-rmt-netapps-competition`|
|[category-5.1.42](category-5.1.42/README.md)|WiFi--设备未配网时反复扫描周边无线AP|`demo-rmt-netapps-competition`|
|[category-5.1.68](category-5.1.68/README.md)|KVDB稳定性vela_kvdb_stability_test02测试|`xts-flat-category-kv-large1144`|
|[category-ble-switch](category-ble-switch/README.md)|开关BLE蓝牙|`xts-flat-bttool`|
|[i2c-c6](i2c-c6/README.md)|I2C 通用主从通信（S31 ↔ C6）|`xts-flat-i2c-c6`|
|[spi-c6](spi-c6/README.md)|SPI 通用主从通信（S31 ↔ C6）|`xts-flat-spi-c6`|
|[ble-gatt](ble-gatt/README.md)|BLE GATT实机读写|`demo-rmt-bttool-coex-pie`|
|[simd-pie](simd-pie/README.md)|SIMD指令集与PIE上下文保护|`demo-rmt-bttool-coex-pie`|
|[jpeg-rgb](jpeg-rgb/README.md)|JPEG 编码/解码与 RGB888/RGB565|`xts-flat-jpeg-rgb`|

历史配置和原始日志见各例程证据链接；本仓公共构建验证范围仍以 [复现验证记录](../../../docs/reproduction/VALIDATION.md) 为准，本次没有重新构建全部 profile。
