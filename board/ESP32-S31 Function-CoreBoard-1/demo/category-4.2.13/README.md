# Audio_DSP--Capture_cmoka_1channels

## 已通过范围

- 4.2.13：🟢 已通过；PASS1143原始命令+用户试听

历史录音为 1 通道、16bit、16000Hz、10 秒；通过结论包括用户试听。录音文件导出后试听，不以文件存在作为声音正常的判据。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/testing/drivers/drivertest/drivertest_audio.c](../_sources/apps/testing/drivers/drivertest/drivertest_audio.c)：原工程 `apps/testing/drivers/drivertest/drivertest_audio.c`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[xts-flat-audio-mono-args](../../configs/xts-flat-audio-mono-args/defconfig)。
- 镜像类型：FLAT（仅 nuttx.bin）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" xts-flat-audio-mono-args 8
```

产物位于 `out/esp32s31-xts-flat-audio-mono-args/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

板载 ES8311/麦克风，先准备有足够空间的可写 `/data`（不要遮盖已有挂载）。历史实测命令如下，向麦克风发声并导出 PCM 后按上文格式试听：

```text
cmocka_driver_audio -a 1 -p /data/mic1143.pcm -s 16000 -c 1 -b 16
```

下面保留原文完整步骤供核对：

1、在nsh中输入 cmocka_driver_audio -a 1 -p /data/1000.pcm -s 16000 -c 1 -b 16
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

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 2852 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [audio1143-mono/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/audio1143-mono/result.json)
- [checkpoint1143-audio-mono/result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1143-audio-mono/result.json)
- [checkpoint1143-audio-mono/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1143-audio-mono/SHA256SUMS)
- [audio1143-mono/uart.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/audio1143-mono/uart.log)
- [logs/xts1143-audio-mono.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/xts1143-audio-mono.log)
- [audio1143-mono/pcm-review.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/audio1143-mono/pcm-review.json)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
