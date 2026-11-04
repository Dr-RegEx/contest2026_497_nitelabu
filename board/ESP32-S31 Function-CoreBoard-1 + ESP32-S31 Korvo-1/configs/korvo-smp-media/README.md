# Korvo SMP MEDIA playback

This configuration extends `korvo-smp-audio` with the standard MEDIA server,
Player API, FFmpeg graph and local Unix sockets. It uses two CPUs in the
S-mode kernel build. Application ELFs and `/etc/media` are embedded in the
kernel image; temporary files use a dedicated 512 KiB PSRAM filesystem heap
excluded from the process page pool.

The application ROMFS uses `/dev/ram0` at `/system/bin`, with distinct
`g_s31_appromfs` symbols in read-only aligned storage. The `/etc` ROMFS uses
`/dev/ram1`; it must not share the application device minor.

Build in a new output directory with the repository build helper and
`--profile korvo-smp-media --workspace-source`. Preserve the original Flash
before deployment, including every sector covered by the resulting image.

From the UART shell, start `/system/bin/mediad &`, then run
`/system/bin/s31mediacheck /tmp/media-check.wav` with a new filename. The check
creates a synthetic WAV, plays it three times through the public Player API,
closes each player and removes its file. It refuses to overwrite an existing
file. After the check exits, use `ps` to identify the idle `mediad` task and
`kill -15 <pid>` to stop it; confirm the service's entire process group exits
before starting it again.

Playback completion verifies the local software and PCM path. Listening,
microphone capture and integration with SystemServer and other applications
require their corresponding checks. This profile starts no network activity
and does not mount removable storage.
