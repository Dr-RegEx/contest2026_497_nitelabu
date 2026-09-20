# Original audio resource preparation 984

September 16, host-only. No UART, provisioning or target acceptance.
The original four files already exist under
`openvela-dev/docs/zh-cn/test_dev_guide/mediatool测试资源和测试步骤/测试资源/音视频测试资源文件/`.
Do not download replacements or alter the originals. Exact paths, lengths and
SHA256 values are recorded in `audio-original-resources-984.json`.

| Original case | File | Bytes | Integration consequence |
|---|---|---:|---|
| 4.1.128 | audio_file.mp3 | 3603915 | MPEG Layer III, 44.1kHz stereo; MP3 decoder/demuxer required. |
| 4.1.129 | audio_file.opus | 1629963 | Ogg Opus; OpusHead reports stereo/input16000Hz. Input rate is not an assertion of decoder output rate. |
| 4.1.130 | audio_file.aac | 1474609 | Actual ISO BMFF/MP4 container; enable MOV demuxer as well as AAC decoder. Filename suffix is insufficient. |
| 4.1.131 | audio_file.wav | 17473937 | Original full file exceeds each16MiB RAM/Flash capacity. Ordinary whole-file tmpfs/Flash placement cannot work. |

Python wave inspection reports PCM16 stereo44100Hz,3970991frames and
90.045147seconds for the WAV declared data chunk. Preserve the complete
17473937-byte original including other chunks; do not trim or transcode it
and label the result the original attachment.

MP3/AAC/Opus can be transferred one at a time into the existing audio-profile
/data tmpfs after checking live available memory. The existing
`audio-file-transfer.py --upload ... --receipt ... --output ...` preserves
the original rb/sb transfer and requires an idle UART. Its upload result checks
length; a subsequent original sb download and host SHA256 comparison are
needed for byte-for-byte transfer evidence. Transfer success is not playback
acceptance. Preserve logs before removing only the file created for that case.

Playback preparation must include local mediad/mediatool and a Music graph
routed to pcm0p, the real demuxers/decoders and sample conversion required by
these attachments. Keep network provisioning disabled. Original commands:
`mediatool`, `open Music`, `prepare 0 url /data/audio_file.<ext>`, `start 0`,
`close 0`, `q`. Normal uninterrupted music/no crash needs actual board and
listening evidence; compilation alone is not a PASS.

The oversized WAV remains pending an appropriate storage/streaming solution
and explicit disclosure if the published local-file setup changes. No new
external storage or networking requirement is silently added to the current
board-only phase.
