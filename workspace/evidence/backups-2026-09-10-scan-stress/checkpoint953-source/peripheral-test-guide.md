# 外围实测安排（9月19日提交）

状态：GPIO固件886、BMI160固件887已编译完成，尚未上板通过。9月17日预留接线实测，
9月18日预留修复、复测和演示材料。当前864长测结束前不要接线、复位、
断电或抢占串口；原估计终点为9月16日16:43（北京时间）；主机时钟基准差异须先完成实际时长审查，不能直接按脚本自动PASS开工。

## 准备物品

- BMI160 六轴传感器模块，支持3.3V电源及3.3V I2C电平。
- 与开发板、模块排针匹配的杜邦线，GPIO回环1根，传感器至少4根；
  若模块没有固定CSB/SDO电平，还需要对应连线。
- 能实际断开开发板所有供电来源的条件，用于10次冷上电计时。
  不能仅用串口RTS复位代替断电；两个USB口或外部电源同时供电时要全部考虑。

## GPIO（独立xts-flat-gpio固件）

断电后连接：J2第13脚（丝印47，GPIO47）与第14脚（丝印48，GPIO48）。
只连接这两个信号脚，不连接3V3或5V。固件注册：

| 接口 | 芯片GPIO | J2编号 | 用途 |
|---|---:|---:|---|
| /dev/gpio0 |47|13|输入及中断|
| /dev/gpio1 |48|14|输出|

原始程序分别验证低/高输出和四种中断配置。实际源码的`-r`含义是
0下降沿、1上升沿、2双沿、3高电平；程序自带帮助文本有误。
原始测试对poll超时没有严格断言，因此验收还要保留驱动真实IRQ记录，
不能只根据cmocka打印PASS就确认中断正常。自动执行器还会要求实际IRQ计数大于0；若未观察到中断，会保留失败，不自动重试。

## BMI160（独立xts-flat-bmi160固件）

| 开发板 | BMI160模块 | 说明 |
|---|---|---|
| J2第35或36脚，3V3 | 对应3.3V电源输入 | 以模块标识确认VCC/VIN/VDD/VDDIO；不要使用5V信号 |
| J2的GND脚，如第33脚 | GND | 共地 |
| J2第15脚，GPIO45 | SCL/SCK | I2C0 SCL |
| J2第16脚，GPIO46 | SDA/SDI | I2C0 SDA |
| 3V3 | CSB（若模块引出且未固定） | I2C模式，按模块电路确认 |
| GND | SDO（若模块引出且未固定） | 选择0x68地址 |

I2C总线需要上拉到3.3V；先确认模块是否已带上拉。
此专用固件把I2C0从板载音频GPIO50/51改到GPIO45/46，不用于板载音频演示。
预期设备为`/dev/accel0`，运行原始：

```text
cmocka_driver_i2c_spi -d /dev/accel0
```

保留原始100次六轴读取输出；模块缺失或读取失败不得算通过。
当前源代码需要BMI160字符接口，不能用uORB节点替代它。

## 实体按钮与冷上电

1. 复位按钮项目按发布用例实际要求按板上RESET，保留从动作到NSH的记录。
2. 冷上电项目做10次真实断电/重上电，记录每次启动耗时和串口启动输出。
3. USB供电与串口桥一起断开时，主机串口重新枚举也有耗时；记录测量方法，
   不把重新打开串口后的时间冒充完整上电时间。正式实测时由主代理协助采集。

## 引脚依据

- [乐鑫官方开发板用户指南：J2引脚表](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-function-coreboard-1/user_guide.html#header-block)
- [官方开发板原理图，第2页](https://dl.espressif.com/schematics/esp32-s31-function-coreboard-1-schematics.pdf)
- 本地发布用例：`openvela-dev/docs/zh-cn/test_dev_guide/openvela_xts_test_cases.md`
- 实际测试：`apps/testing/drivers/drivertest/drivertest_gpio.c`和`drivertest_i2c_spi.c`

现场以开发板丝印、模块实际型号和接线照片核对位置，J2编号不等于GPIO编号。

## 已准备的执行命令（实测时使用，不在长测期间执行）

以下命令在工作区根目录执行。先断电接线并确认引脚，再按项目烧录和运行；
每次输出保存到新的编号日志。两套FLAT固件只写内核槽，保留现有AppFS和持久分区。

```sh
S31_FLAT_PROFILE=xts-flat-gpio bash backups/2026-09-10-scan-stress/flash-xts-flat.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build886-flat-gpio.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-peripheral.py gpio --receipt backups/2026-09-10-scan-stress/build886-flat-gpio.sha256 --wiring-confirmed

S31_FLAT_PROFILE=xts-flat-bmi160 bash backups/2026-09-10-scan-stress/flash-xts-flat.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build887-flat-bmi160.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-peripheral.py bmi160 --receipt backups/2026-09-10-scan-stress/build887-flat-bmi160.sha256 --wiring-confirmed
```

执行器检查固件SHA256、配置和串口空闲，再复位一次；不会自动重试或刷机。
传入`--wiring-confirmed`表示操作者已经实际核对上述接线，不能代替接线检查。

## 品类 Sensor 框架（新增896，复用BMI160接线）

普通I2C字符测试887之后，保持上述BMI160接线，另刷896执行原始品类4.1.119。
两个主题为 `sensor_accel_uncal0,sensor_gyro_uncal0`；25/50/100Hz各执行，完整重复10轮。
`-n 10`是两个主题合计10条，执行器同时要求每个主题都收到数据。

```sh
S31_FLAT_PROFILE=xts-flat-bmi160-uorb bash backups/2026-09-10-scan-stress/flash-xts-flat.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build896-flat-bmi160-uorb.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-peripheral.py bmi160_uorb --receipt backups/2026-09-10-scan-stress/build896-flat-bmi160-uorb.sha256 --wiring-confirmed
```

当前896仅编译通过，未连接传感器实测；不能记品类PASS。
PWM品类还需要示波器或逻辑分析仪保存波形图。音频951已含I2S/ES8311及原有nx工具，仍未上板；不能把编译完成视为声音验收。

## PWM 波形（新增898）

独立 `xts-flat-pwm` 固件在 GPIO48/J2第14脚输出，探头接该脚、地线接开发板GND。
准备示波器或逻辑分析仪，输入适配3.3V。原始 `cmocka_driver_pwm` 默认100Hz、50%、5秒。
保存频率/占空比读数和波形图，并与同次UART日志关联；只有软件API返回PASS不够。
不要同时保留不需要的GPIO47/48回环连线。实际启动前先设置好捕获触发。

## 文件系统真实掉电（当前949，替代905）

使用 `xts-flat-category-fs-flash` 固件，独立测试区 `/dev/xtsflash` 为Flash
0xc00000起、1MiB大小。先由主代理完成该区双份备份和946原始块测试，
然后才格式化并挂载测试 `/data`；禁止使用生产 `/apps`。

原始三个程序分别为：`power_off_test01 /data`（写入中断电）、
`power_off_test02 /data`（创建中断电）、`power_off_test03 /data`（空间变化）。
每类按原文完成3轮断电/恢复并保存结果。恢复后重新挂载同一分区，
不能带 `forceformat` 或 `autoformat`，否则会抹掉待验证的持久化证据。
重启后程序校验完成并退出时，应先保存该轮证据，再准备下一轮新测试状态。

三类掉电与公共10次冷启动的判据不同；可以在现场协调供电操作顺序，
但不能省略任一原始用例的次数、断电时机或结果记录。

## 9月16日新增准备与接线顺序

这些候选均未上板；以下是17日现场准备，不要求现在接线或打断864。

| 项目 | 当前候选 | 实物与验收重点 |
|---|---:|---|
| GPIO回环 |886|47与48短接；真实电平和中断记录|
| BMI160字符/uORB |887/896|45/46及3.3V电平；100次六轴读取、三种频率各10轮|
| PWM |898|拆除47/48回环，示波器或逻辑分析仪测48；保存波形|
| ADC |952|47接稳定电压源并共地，用万用表记录实际输入；仍缺校准换算，不宣称毫伏准确度|
| 音频 |951|板载麦克风、计划书列的4Ω/3W外接扬声器及匹配接口；真实语音文件录放和听音|
| BLE |939|具备BLE扫描工具的手机/对端；多设备性能项另需相应数量的设备和原始次数|
| USB/ADB |945|原生J4 Type-A需要经核实的设备连接与VBUS隔离；两个Type-C口不能替代|
| 文件系统掉电 |949|测试区已双备份后按三类各3轮执行；恢复时不能自动格式化|

建议先GPIO，拆除回环后PWM，再ADC；GPIO47不能同时连到GPIO48输出和外部电压源。
BMI160保持自己的45/46接线，但音频固件使用板载50/51，不能混用两个I2C配置。
音频当前16bit、半双工；原文3ch/32bit录音及同时录放回环仍未支持。
ADC提供17位寄存器原始字段，不代表17位模拟精度，也不等于原文“预期电压”已通过。
USB接线详见usb-adb-target-sequence.md，不使用普通双供电主机口直连方案。

详细限制和证据入口：checkpoint951-audio-tools.md、ble-target-sequence.md、
checkpoint952-adc-raw.md、checkpoint946-flash-psram-stack.md。
所有刷机、串口操作和供电切换由一个执行者串行完成；可并行的是离线代码审查和资料准备。
