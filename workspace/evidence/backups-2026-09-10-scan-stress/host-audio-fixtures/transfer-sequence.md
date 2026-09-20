# Audio transfer transport (prepared, not target-tested)

Use audio-file-transfer.py only after longrun864 actual24h review and any
same-boot extension finish. First flash audio968 and verify its boot. Create
and mount a disposable /data tmpfs if not already present; the helper requires
the existing mount and will not mount or format anything itself.

Upload the prepared fixture to a fresh target filename:

```sh
python3 backups/2026-09-10-scan-stress/audio-file-transfer.py --upload backups/2026-09-10-scan-stress/host-audio-fixtures/hi-idf-mono16-48k.pcm --receipt backups/2026-09-10-scan-stress/build968-flat-audio.sha256 --output backups/2026-09-10-scan-stress/audio-upload-first
```

The output directory must be fresh. A preexisting target filename is refused;
choose a new source basename intentionally rather than deleting saved data.
Use nxplayer's raw mode with mono16/48000 for this particular fixture.

Record using the original nxrecorder or cmocka command and stop normally.
Do not flash/reset after recording. Release the command logger's serial handle
before invoking the transfer helper to export the existing recording:

```sh
python3 backups/2026-09-10-scan-stress/audio-file-transfer.py --download mic.pcm --receipt backups/2026-09-10-scan-stress/build968-flat-audio.sha256 --output backups/2026-09-10-scan-stress/audio-capture-first
```

--download accepts a basename under /data. The example assumes the recorder
actually wrote /data/mic.pcm; the original sequential test may use a different
name. The helper records actual file size, received size, host digest and UART
commands, and leaves raw PCM under the fresh receive/ directory. Wrap it with
its actual recorded channel/rate/bit-depth metadata on the host before listening.

The helper opens the already configured raw115200 tty with os.open; it makes
no DTR/RTS or termios setting changes. OS-induced line changes cannot be ruled
out. Boot markers or a missing RAM recording are failures, not permission to
reset or recreate the file. Preserve such failure evidence. Transfer completion
is not microphone/speaker quality acceptance and is not an xTS audio PASS.

Validation so far: Python syntax and source review only. No UART was opened,
no reset was sent, no target file was created or transferred by this helper.

## Original sequential case and export with one serial handle

The preferred original4.2.15 execution keeps the same serial handle from
capture through playback and PCM export, with the original44100/stereo16/10s
defaults. Select a fresh /data basename and host output directory:

```sh
python3 backups/2026-09-10-scan-stress/audio-file-transfer.py --sequential xts-sequential.pcm --receipt backups/2026-09-10-scan-stress/build968-flat-audio.sha256 --output backups/2026-09-10-scan-stress/audio-sequential-first
```

The helper reads actual Umem capacity and requires2MiB free/contiguous space
before the nominal1,764,000-byte recording plus task/buffer/filesystem overhead.
It then runs the unchanged `cmocka_driver_audio -a 3 -p /data/xts-sequential.pcm`,
retains both Start messages and the original cmocka result, and exports through
original sb without closing/reopening the serial handle. No duration reduction,
format override, automatic rerecord or reset is performed.

Result metadata includes program runtime, actual PCM length and duration from
sample count. Audio acceptance remains LISTENING_AND_FORMAT_REVIEW_PENDING;
real microphone/speaker quality and any timing discrepancy need review.
Syntax/help/source validation only; this mode has not been run on the board.
