# 1778 音频播放边界审计

日期：2026-09-19

本记录只核对比赛清单中的 `4.2.10` 和 `4.2.15`。当前没有人工听验条件，也没有刷机、串口或外接扬声器操作；因此本记录不新增 xTS 通过项。

## 现有实物前置证据

| 项目 | 已有证据 | 本次结论 |
|---|---|---|
| 4.2.10 Playback_cmoka_1channels | `audio-playback1708/result.json` 标记 `PROGRAM_PASS`；`uart.log` 的原始 `cmocka_driver_audio -a 2 -p /data/xts-sequential.pcm` 为 `1 test(s) PASSED` | 程序路径通过；声音仍需 J9 外接扬声器听验 |
| 4.2.15 Looper_cmoka | `audio1059-sequential/result.json` 的原始 `cmocka_driver_audio -a 3` 完成，录音 1769472 字节；`listening-result.json` 只确认麦克风讲话清晰、无明显电流噪声 | 录音和程序路径通过；播放声音仍需 J9 外接扬声器听验 |

## 源码边界核对

- `boards/.../src/esp32s31_audio.c` 使用 I2C0、ES8311 地址 `0x18`、100 kHz，并注册 `/dev/audio/pcm0p` 与 `/dev/audio/pcm0c`。
- 同文件的功放控制使用 GPIO57；I2S TX 开始时由 `esp32s31_audio_pa(true)` 使能，停止时关闭。J9 是功放输出连接点，板上没有集成扬声器。
- `arch/risc-v/src/esp32s31/esp32s31_i2s_duplex.c` 的 TX/RX 环形 DMA、完成回调、FINAL 排空和错误回收路径已有历史构建及上板记录；本轮未发现可在无硬件条件下安全改变听感或播放验收结论的缺口。

## 构建核验边界

历史镜像 `openvela-dev/out/esp32s31-xts-flat-audio-final/nuttx.bin` SHA256 为
`b606b865436f849cfcf77e747b69a63bd0b261cf525c66d3af376d11a5d49969`，与 1059 证据中的 `build1057-flat-audio-final.sha256` 一致。

尝试对旧输出目录增量运行 Ninja 时，CMake 要求先 `distclean previous make build`；没有执行破坏性清理，也没有把该失败当作音频源码失败。`git diff --check` 通过。

## 状态

本轮无源码修复、无新增通过。清单中的 4.2.10 与 4.2.15 继续保持“程序/录音通过，待 J9 外接扬声器听验”；下一步必须由人工接扬声器并试听，才能闭合播放验收。
