##### 4.2.12 Audio_DSP--Capture_cmoka_2channels

**测试目的：** 二、品类自测用例 > 4、功能测试 > 4.2 驱动BSP

**前提条件：** 文件系统, 测试码流
声道,  采样深度,  采样率尽可能覆盖


**步骤：**

1、在nsh中输入 cmocka_driver_audio -a 1 -p /data/1000.pcm (2channels, 44100, 16bits)
注：
1）cmocka_driver_audio需要两个必填参数：
-a：1表示只采集，2表示只播放，3表示先采集再播放
-p：文件名，如 -p /data/1.pcm
2）默认音频设备为/dev/audio/pcm0p, /dev/audio/pcm0c，使用 -i 更改录制音频设备，如 -i /dev/audio/pcm1c；使用 -i 更改播放音频设备。例如 -i /dev/audio/pcm1p。
3）默认格式是AUDIO_FMT_PCM 使用-f改变格式，如 -f mp3
4）默认采样率为 44100，通道为 2，bps 为 16：使用 -s 更改采样率；使用 -c 改变频道；使用 -b 更改 bps。
5）默认记录时间为10s。使用-t 来改变

**预期结果：**

录音文件声音正常

---
