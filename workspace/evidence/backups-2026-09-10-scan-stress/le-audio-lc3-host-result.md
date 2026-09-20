# LE Audio LC3 编解码主机验证

日期：2026-09-19。使用工程既有 `openvela-dev/external/liblc3/liblc3` 源码，没有下载或替换 SDK。此结果不代表无线传输或实际音频设备通过。

真实LC3编解码回归：16kHz单声道PCM，10ms帧，40字节/帧（32kbit/s），100帧440Hz信号。编码和解码均返回0，算法延迟40样本；按库报告的延迟对齐后，测试信号SNR为19.27dB。这是该合成信号的主机结果，不是芯片性能/实际听感或xTS验收门槛。

首次UBSan发现 `src/fastmath.h` 的指数拼接使用有符号左移：`left shift of 154664958 by 23 places cannot be represented in type 'int'`。已将指数位的移位与相加改为uint32_t模2^32运算，保持原IEEE754位拼接意图，避免有符号溢出未定义行为。没有关闭检测绕过问题。

复现：`python3 tools/tests/le-audio-lc3/run.py`，编译项目真实codec全部C源和 `roundtrip.c`，启用ASan/UBSan及 `-fno-sanitize-recover=all`。修复后退出0、100帧通过且无sanitizer报错，日志 `le-audio-lc3-host-test.log`。

后续已完成NuttX `CONFIG_LIB_LC3` 与ZBlue `CONFIG_LIBLC3` 兼容映射及S31完整交叉链接，见 [四角色与LC3接入](ble-audio-roles-lc3-host-result.md)。实际CIS/BIG及音频对端传输仍待验证。主机回归不计新增xTS通过。
