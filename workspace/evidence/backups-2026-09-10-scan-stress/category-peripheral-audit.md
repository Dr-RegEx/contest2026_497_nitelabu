# 品类外围、音频、GUI、Bluetooth xTS只读审计

日期：2026-09-15。依据本地原始 `openvela-dev/docs/zh-cn/test_dev_guide/openvela_xts_test_cases.md`，保留标题及行号，重复4.2.13以标题/行号区分。此报告不含存储、升级及纯网络（另行审计），通用25/35不是完整精简集的通过率。本次只写本文件，未操作串口/复位/固件/源码/构建。

证据：板README记载GPIO61电气中断、GPIO60 RGB、ES8311 I2C ID及LAN演示。checkpoint888记载GPIO886/BMI160887仅构建及主机检查、尚未实物测试。build883配置无音频/PWM/USB ADB/显示，build887明确关闭SENSORS_BMI160_UORB。默认BT_NATIVE=y不等于控制器/蓝牙服务/bttool已经接通。

“软件未适配”说明板上或芯片有能力，但openvela接口缺失；“适用性待确认”保留未选外设/产品配置，不擅自删除为N/A。硬件缺独立通道和测试仪器未到位分开陈述。

| 原始标题 | 原始行号 | 适用性、状态和缺口 |
| --- | ---: | --- |
| 4.2.9 Audio_DSP--Playback_cmoka_2channels | 2350 | 软件未适配，双通道适用性须区分数字格式和独立物理通道：ES8311单ADC/DAC；驱动接受部分2声道格式不等于双路独立采播。无S31 I2S/音频节点或原用例证据。 |
| 4.2.8 Audio_DSP--Playback_nxplayer | 2376 | 板载能力适用，软件未完成：ES8311、麦克风、NS4150B、扬声器接口存在；仅I2C ID通过。缺S31 I2S TX/RX、codec注册/音频节点。播放需扬声器听音，录音可用板载麦克风；原音频用例未测。 |
| 4.2.7 Touch Panel测试2 | 2396 | 适用性待确认：板上无触摸屏；未选外置器件/接线，无本板touchscreen绑定，未测。 |
| 4.2.6 Touch Panel测试1 | 2435 | 适用性待确认：板上无触摸屏；未选外置器件/接线，无本板touchscreen绑定，未测。 |
| 4.2.5 Lcd测试 | 2462 | 适用性待确认：无板载显示屏、已选外置面板或本板LCD/FB/LVGL显示输入后端；未测，LAN网页不是屏幕证据。 |
| 4.2.4 Framebuffer测试 | 2499 | 适用性待确认：无板载显示屏、已选外置面板或本板LCD/FB/LVGL显示输入后端；未测，LAN网页不是屏幕证据。 |
| 4.2.3 2D-GPU测试 | 2536 | 软件未适配，显示配置待确认：芯片有PPA/2D-DMA，不能称硬件无2D加速；缺NuttX/LVGL加速接入和实测benchmark。 |
| 4.2.25 AC 底座 | 2553 | 适用性待确认：原文针对R528/UART4/COMLITE AC底座；本板没有对应底座或协议绑定，未测。 |
| 4.2.24 电源按键 | 2585 | 部分相关证据，原项未完成：本板有BOOT/RESET，未证实独立电源键；GPIO61 /dev/buttons 电气10边沿不等于原文event1电源键 Sample=4/0。需确认映射及人工按键。 |
| 4.2.23 马达 | 2617 | 适用性待确认：无板载/已选外置LRA马达和力反馈驱动，/dev/lra0未接通，5s震动未测。 |
| 4.2.22 Relay | 2647 | 适用性待确认：无板载/已选外置继电器和relay下半部；GPIO回环不能替代原用例。 |
| 4.2.20 PWM | 2677 | 软件未适配 + 波形待测：芯片有LEDC/MCPWM；common esp_ledc.c可参考复用，但S31构建源/时钟HAL兼容及/dev/pwm0注册未完成。原cmocka_driver_pwm和厂商波形图均缺，RMT/RGB不是PWM通过。 |
| 4.2.2 USB（ADB）测试 | 2708 | 软件未适配：板上有USB功能，但缺S31 USB device controller/端点和ADB gadget接入；CP2102N UART或固定USB Serial/JTAG不等于USB ADB。OTG Type-A默认host，设备模式连接需核对。 |
| 4.2.17 PMU ADC | 2724 | 软件未适配、PMU限定适用性待确认：芯片有SAR ADC，本板未确认独立PMU ADC；缺S31 NuttX ADC下半部/安全通道，也缺稳定电压与共地实测，不能只写缺仪器。 |
| 4.2.16 Audio | 2749 | 板载能力适用，软件未完成：ES8311、麦克风、NS4150B、扬声器接口存在；仅I2C ID通过。缺S31 I2S TX/RX、codec注册/音频节点。播放需扬声器听音，录音可用板载麦克风；原音频用例未测。 |
| 4.2.15 Audio_DSP--Looper_cmoka | 2779 | 板载能力适用，软件未完成：ES8311、麦克风、NS4150B、扬声器接口存在；仅I2C ID通过。缺S31 I2S TX/RX、codec注册/音频节点。播放需扬声器听音，录音可用板载麦克风；原音频用例未测。 |
| 4.2.14 Audio_DSP--Looper_nxlooper | 2805 | 软件未适配，拓扑待确认：原pcm0p+pcm13c的2ch回环未建立；板载单物理采播链不能直接等价。缺I2S双向链路、codec和音频队列，未测。 |
| 4.2.13 Audio_DSP--Capture_cmoka_3channels | 2826 | 软件缺口 + 原文通道能力不匹配：ES8311/板载麦克风为单ADC通道，原文3ch/32bit/pcm13c不是增加测试线能补齐；缺S31 I2S RX/codec绑定。保留原3通道需另有多通道采集硬件或确认板型映射，不能伪造通道。 |
| 4.2.13 Audio_DSP--Capture_cmoka_1channels | 2852 | 板载能力适用，软件未完成：ES8311、麦克风、NS4150B、扬声器接口存在；仅I2C ID通过。缺S31 I2S TX/RX、codec注册/音频节点。播放需扬声器听音，录音可用板载麦克风；原音频用例未测。 |
| 4.2.12 Audio_DSP--Capture_cmoka_2channels | 2878 | 软件未适配，双通道适用性须区分数字格式和独立物理通道：ES8311单ADC/DAC；驱动接受部分2声道格式不等于双路独立采播。无S31 I2S/音频节点或原用例证据。 |
| 4.2.11 Audio_DSP--Capture_nxrecorder | 2904 | 软件缺口 + 原文通道能力不匹配：ES8311/板载麦克风为单ADC通道，原文3ch/32bit/pcm13c不是增加测试线能补齐；缺S31 I2S RX/codec绑定。保留原3通道需另有多通道采集硬件或确认板型映射，不能伪造通道。 |
| 4.2.10 Audio_DSP--Playback_cmoka_1channels | 2925 | 板载能力适用，软件未完成：ES8311、麦克风、NS4150B、扬声器接口存在；仅I2C ID通过。缺S31 I2S TX/RX、codec注册/音频节点。播放需扬声器听音，录音可用板载麦克风；原音频用例未测。 |
| 4.1.133 Video H265 解码播放 | 4321 | 条件适用待确认：原文不支持media_tool芯片可免测；当前无S31 media pipeline、解码/显示/音频通路和屏幕。不能仅因当前未启用MEDIA就判定芯片不支持，未测。 |
| 4.1.132 Video H264 解码播放 | 4354 | 条件适用待确认：原文不支持media_tool芯片可免测；当前无S31 media pipeline、解码/显示/音频通路和屏幕。不能仅因当前未启用MEDIA就判定芯片不支持，未测。 |
| 4.1.131 Audio wav 解码播放 | 4387 | 条件适用待确认、软件未适配：原文media_tool免测条件须核实，本板确有音频硬件。缺I2S/codec、media及相应解码播放集成，I2C ID不是声音输出证据。 |
| 4.1.130 Audio aac 解码播放 | 4420 | 条件适用待确认、软件未适配：原文media_tool免测条件须核实，本板确有音频硬件。缺I2S/codec、media及相应解码播放集成，I2C ID不是声音输出证据。 |
| 4.1.129 Audio opus 解码播放 | 4482 | 条件适用待确认、软件未适配：原文media_tool免测条件须核实，本板确有音频硬件。缺I2S/codec、media及相应解码播放集成，I2C ID不是声音输出证据。 |
| 4.1.128 Audio mp3 解码播放 | 4515 | 条件适用待确认、软件未适配：原文media_tool免测条件须核实，本板确有音频硬件。缺I2S/codec、media及相应解码播放集成，I2C ID不是声音输出证据。 |
| 4.1.127 Audio--nxlooper回环功能测试 | 4548 | 软件未适配，拓扑待确认：原pcm0p+pcm13c的2ch回环未建立；板载单物理采播链不能直接等价。缺I2S双向链路、codec和音频队列，未测。 |
| 4.1.126 Audio-- nxplayer播放功能测试 | 4580 | 板载能力适用，软件未完成：ES8311、麦克风、NS4150B、扬声器接口存在；仅I2C ID通过。缺S31 I2S TX/RX、codec注册/音频节点。播放需扬声器听音，录音可用板载麦克风；原音频用例未测。 |
| 4.1.125 Audio--nxrecorder录音功能测试 | 4611 | 软件缺口 + 原文通道能力不匹配：ES8311/板载麦克风为单ADC通道，原文3ch/32bit/pcm13c不是增加测试线能补齐；缺S31 I2S RX/codec绑定。保留原3通道需另有多通道采集硬件或确认板型映射，不能伪造通道。 |
| 4.1.124 GUI--lvgldemo widget组建基本功能测试 | 4645 | 适用性待确认：无板载显示屏、已选外置面板或本板LCD/FB/LVGL显示输入后端；未测，LAN网页不是屏幕证据。 |
| 4.1.123 GUI--lvgldemo压力测试 | 4673 | 适用性待确认：无板载显示屏、已选外置面板或本板LCD/FB/LVGL显示输入后端；未测，LAN网页不是屏幕证据。 |
| 4.1.122 GUI--lvgldemo music基本功能测试 | 4700 | 适用性待确认：无板载显示屏、已选外置面板或本板LCD/FB/LVGL显示输入后端；未测，LAN网页不是屏幕证据。 |
| 4.1.121 GUI--lvgldemo keypad屏上操作测试 | 4734 | 适用性待确认：无板载显示屏、已选外置面板或本板LCD/FB/LVGL显示输入后端；未测，LAN网页不是屏幕证据。 |
| 4.1.120 GUI--frame buffer功能测试 | 4761 | 适用性待确认：无板载显示屏、已选外置面板或本板LCD/FB/LVGL显示输入后端；未测，LAN网页不是屏幕证据。 |
| 4.1.119 Sensor驱动框架测试 | 4782 | 软件缺口 + BMI160待接：build887只是/dev/accel0字符接口，明确关闭SENSORS_BMI160_UORB，未启用USENSOR/UORB/LISTENER。原要求accel+gyro双topic在25/50/100Hz各10轮；现有bmi160_uorb.c可复用但有读错误/时间戳必要修复。 |
| 开关BLE蓝牙 | 4968 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 开关状态首先是软件缺口，不依赖额外广播夹具。 |
| adv start -i <interval>指定广告间隔 | 4990 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需手机NRF核对20/100/1000/10240ms间隔，误差≤5%。 |
| 相同public地址多路广播 | 5023 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需手机NRF/广播对端观察；多地址/多实例须按真实控制器能力实现。 |
| 4.1.105 蓝牙--BLE&Wi-Fi共存（Zblue协议栈）-蓝牙扫描和wifi共存 | 5150 | 延期未删除：S31具备Bluetooth/Wi-Fi共存能力，未接通NuttX S31控制器/HCI/协议栈/bttool。原文步骤虽以Wi-Fi为主，纯Wi-Fi循环不能冒充BLE共存；需真实BLE扫描/广播背景加原100次。 |
| 4.1.104 蓝牙--BLE&Wi-Fi共存（Zblue协议栈）-蓝牙广播和wifi共存 | 5188 | 延期未删除：S31具备Bluetooth/Wi-Fi共存能力，未接通NuttX S31控制器/HCI/协议栈/bttool。原文步骤虽以Wi-Fi为主，纯Wi-Fi循环不能冒充BLE共存；需真实BLE扫描/广播背景加原100次。 |
| 相同random地址多路广播 | 5388 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需手机NRF/广播对端观察；多地址/多实例须按真实控制器能力实现。 |
| 不同地址多路广播 | 5417 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需手机NRF/广播对端观察；多地址/多实例须按真实控制器能力实现。 |
| scan设置扫描phy | 5446 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 原文2M扫描注明研发确认，不能把2M主广播扫描支持当已确定能力。 |
| scan设置扫描mode | 5474 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需手机NRF/广播对端观察；多地址/多实例须按真实控制器能力实现。 |
| 组合扫描场景 | 5502 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 原文2M扫描注明研发确认，不能把2M主广播扫描支持当已确定能力。 |
| BLE发现时长 | 5528 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需手机/计时或录屏，原10轮；部分门槛未给数值，应保留测量值。 |
| BLE被发现时长 | 5548 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需手机/计时或录屏，原10轮；部分门槛未给数值，应保留测量值。 |
| BLE发现成功率 | 5572 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需手机NRF和20轮，屏蔽室100%或家居≥95%。 |
| BLE被发现成功率 | 5591 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需手机NRF和20轮，屏蔽室100%或家居≥95%。 |
| BLE发现5个设备成功率 | 5610 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需5台手机和20轮；按原场景成功率验收。 |
| BLE配对成功率 | 5629 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需手机NRF和20轮，屏蔽室100%或家居≥95%。 |
| BLE扫描效率 | 5648 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需广播/扫描对端及≤1m、5m、10m场地，给出真实收包成功率。 |
| 广播与扫描共存后扫描效率 | 5671 | 延期未删除：芯片支持Bluetooth，但NuttX S31控制器OS适配/HCI/上层bttool未接通，无本条上板证据。 另需广播/扫描对端及≤1m、5m、10m场地，给出真实收包成功率。 |

共 **55 个原标题条目**，全部未完成或适用性待确认，本审计新增 **0 PASS**。


## 长测期间最多两项可推进的软件适配

1. **4.2.20 PWM**：复用 `nuttx/arch/risc-v/src/common/espressif/esp_ledc.c/.h` 的 `esp_ledc_init(0)` 与 `pwm_register`。补齐S31构建接入，并核对pinned HAL `esp_hal_ledc/esp32s31/ledc_periph.c`、`hal/ledc_ll.h`、时钟复位/GPIO矩阵。独立FLAT配置只选一个空闲J2 GPIO，不占用其他profile；原drivertest_pwm使用SETCHARACTERISTICS/START/STOP，默认不要求pulsecount，无需扩展MCPWM/马达。长测后原用例和示波器/逻辑分析仪波形图才能补齐验收，构建不是PASS。
2. **4.1.119 Sensor驱动框架**：保留build887字符接口；另设BMI160-uORB配置，启用USENSOR/SENSORS/SENSORS_BMI160/SENSORS_BMI160_UORB/UORB/UORB_LISTENER，调用已有`bmi160_register_uorb(0,i2c)`注册accel_uncal/gyro_uncal，复用GPIO45/46和0x68夹具。现有`bmi160_uorb.c`约471/512行worker未检查读错误，3字节sensor-time读入未初始化uint32后右移8会读未初始化高字节；约744行注册仅DEBUGASSERT且继续。须传播真实失败、正确解码并按uORB单位契约处理时间戳。这是该原case必需修复。目标仍是25/50/100Hz双topic各10轮，不能拿字符设备100读替代。

## 关键技术证据与边界

- **音频**：`nuttx/drivers/audio/es8311.c`约823/843/862行声明mono，依赖I2S_RXDATAWIDTH/TXDATAWIDTH、GETMCLKFREQUENCY、RXSAMPLERATE/TXSAMPLERATE、SEND/RECEIVE。本次检查S31和RISC-V common目录未有I2S下半部；Xtensa `esp32s3_i2s.c`仅可参考，不能直接使用另一芯片寄存器/GDMA。pinned IDF有S31 `esp_hal_i2s/esp32s31/include/hal/i2s_ll.h`及i2s_periph，但时钟、异步DMA/中断、队列、板级codec注册均是实际底层工作。因此音频不是只缺扬声器；麦克风已经板载。GPIO57 PA在只读审计中保持原状。
- **Bluetooth**：已有C3 `esp_ble.c/esp_ble_adapter.c`不是S31 ABI支持证据。pinned IDF有 `bt/controller/esp32s31/bt.c`、S31控制器库/PHY btbb，目前未接入本板NuttX。控制器生命周期/内存/锁/中断、HCI、服务命令和共存都需实际完成。手机只能补观察端。蓝牙继续延期、不删除范围。
- **USB ADB**：已有`nuttx/drivers/usbdev/adb.c`与`apps/system/adb`，缺的是S31可用USB设备控制器和连接方案；USB Serial/JTAG为固定功能，CP2102N是UART桥。OTG默认host的Type-A端口需要核对设备模式与供电，TCP adb不能默认算原USB题通过。
- **PPA/ADC**：用户芯片原始规格书明确列PPA/2D-DMA和双12bit SAR ADC，不能标芯片无此能力；缺的是NuttX/LVGL/ADC接入及板上安全使用方案。独立PMU器件未确认，仅PMU限定适用性保留。
- **显示/触摸/relay/马达/AC底座**：官方板载清单无对应器件；未选外扩产品配置，保留适用性待确认/未测，不增加通过数。

参考读取：本地原始xTS、板README、checkpoint888-peripherals-prepared.md、build883/build887实际.config、本文列明驱动源码；硬件为用户提供芯片规格书及之前核对的官方板指南/原理图V1.0。未修改原始资料。

September16 updates supersede the older software-gap wording above: audio951 includes real I2S/ES8311 plus nxplayer/nxrecorder; BLE939 includes controller/OSAL/Zblue/bttool; USB945 includes UTMI Device and adb shell; ADC952 includes raw ADC1 lower half. All BUILD ONLY, no category PASS. Remaining distinctions: audio half-duplex/16bit/mono codec, BLE no Wi-Fi coexistence, USB J4 VBUS fixture and enumeration, ADC no calibrated voltage mapping. See numbered checkpoints.

September16 01:36 latest candidates: audio963 adds original rb/sb file transfer, USB965 explicitly selects pinned16-bit UTMI and PHY timeout calibration, ADC954 adds analog bus power sequencing; BLE939 unchanged. All BUILD ONLY. Earlier timestamped notes retain their historical candidates.

September16 05:15: audio968 now supports original44100Hz using11.2896MHz
MCLK; original48k and earlier supported rates retain12.288MHz. BUILD ONLY.
Original4.2.16 specifies cmocka_driver_audio with no arguments. Current
unaltered app initializes direction=0, opens neither audio endpoint in setup,
and skips both capture/playback iterations; a printed PASS is therefore not
hardware coverage. Preserve literal-step output but clearly distinguish this
limitation from actual4.2.15 sequential recording/playback evidence. Do not
modify the published test to conceal the mismatch. Full duplex and mediatool
integration remain separate gaps. See current category ledger and968 README.
