# Audio963 — original YMODEM transfer, BUILD ONLY

Replaces951 by enabling existing SYSTEM_YMODEM and4096-byte task stacks.
Original rb/sb, nxplayer/nxrecorder and the I2S/ES8311 driver are linked.
No driver or protocol implementation changed. Image441600 bytes; frozen
checkpoint963-flat-audio, archive, build receipt and SHA256SUMS-xts963.
Dependency locks, diff --check, helper shell syntax and build passed.
951 receipt points to its byte-identical frozen image.

This closes the missing file-transfer prerequisite for actual speech playback
and exporting onboard microphone recordings. It is not an audio target PASS.
Use one serial owner for commands and YMODEM, reusing the repository sbrb.ymodem
API already used in xts-ymodem.py. Do not run that old whole test runner on this
profile: it resets the board and expects network/kernel-demo commands.
Do not reset between recording and exporting: audio files live in RAM.

After864 releases UART, boot963, verify /dev/audio/pcm0p and pcm0c, and mount a
fresh disposable tmpfs at /data if not present. Import a real mono signed16-bit
48kHz speech file with original `rb -f /data`, verify its exact byte size, and
perform the original nxplayer playback from checkpoint951-audio-tools.md.
Do not count a generated tone or silent file as speech speed/pitch evidence.

For the board's actual mono microphone path, invoke:

```text
nxrecorder
device /dev/audio/pcm0c
recordraw /data/mic.pcm 1 16 48000
stop
q
sb /data/mic.pcm
```

Allow a measured short interval of real speech between recordraw and stop;
record the interval and exported length. Wrap exported raw samples in a WAV
header on the host with one channel,16-bit little-endian and48000Hz. Listen
to the actual capture and retain it with UART logs. This supports board audio
qualification, not the original3-channel/32-bit recorder case. Simultaneous
capture/playback remains unsupported. Stop capture before starting playback.
All physical wiring and speaker checks remain in peripheral-test-guide.md.

03:17 source audit: nxlooper is not a configuration-only addition. The
I2S transport has one queue/bounce buffer and stops both DMA directions; the
two ES8311 instances share hardware, configure resets the codec, and stop
powers down shared ADC/DAC blocks. Full duplex therefore needs coordinated
codec lifecycle and separate directional transport state. Preserve963 for
first independent playback/capture qualification; no unverified duplex patch
was added. The mediatool WAV/AAC/MP3/Opus cases likewise cannot be replaced by
nxplayer PCM playback: the published steps explicitly use mediatool.

## Original sequential cmocka case (prepared, TARGET NOT RUN)

Original4.2.15 explicitly defines `-a 3` as capture followed by playback.
It does not require simultaneous I2S streams. Candidate963 includes the
unaltered cmocka_driver_audio program. The original document permits `-s`,
`-c`, `-b` selection; record any selected format in the acceptance evidence.
For the supported 48 kHz / two-slot / 16-bit format, retain default10 seconds:

```text
cmocka_driver_audio -a 3 -p /data/xts-sequential.pcm -s 48000 -c 2 -b 16
```

Check that the fresh path does not contain user data: the original test opens
it with O_TRUNC. A nominal10-second file is1,920,000 bytes, plus queued-buffer
and allocator overhead. Verify live free heap before running. This FLAT
profile enables SPIRAM_USER_HEAP and FS_HEAPSIZE=0, so RAM file storage uses
the shared user heap; configuration alone is not proof of available capacity.
Do not shorten the original duration merely to conceal a storage failure.
The board codec is mono: two I2S slots do not establish two independent
physical microphone channels. Retain actual channel data and this limitation.

Capture starts before playback, with cleanup between them. Keep the complete
cmocka result, both Start messages, file length and exported PCM. Actual sound
quality still needs listening; a printed PASS alone is not the published
“normal playback” criterion. Use a bounded host wait because the original
mq_receive has no timeout when the driver stops returning buffers. A hang
must be retained as a failure, not silently reset and retried as success.

Full-duplex review is recorded separately in audio-duplex-feasibility.md.
No audio source/configuration changed during this review.

05:08 update: candidate968 supersedes963 with original44.1kHz support; see
checkpoint968-flat-audio/README.md.963 image/source remain frozen, and its
receipt now points to checkpoint963-flat-audio/nuttx.bin. The48k transfer
procedure above remains applicable. Do not interpret this build as target PASS.

Existing source fixture is now available in host-audio-fixtures/: the pinned
IDF hi_idf_audio.wav and its unchanged mono16/48k PCM payload191648 bytes.
Use the original WAV as the host listening reference; metadata/hash checks
do not establish speech quality or target playback. Source origin and hashes
are retained in manifest.json. See host-audio-fixtures/README.md.

The dedicated audio-file-transfer.py transport is now prepared for upload
and export without requesting a reset. See host-audio-fixtures/transfer-sequence.md.
Only syntax/source checks were performed; no target transfer has run.
