# Audio tools candidate951 — BUILD ONLY

Extends the frozen916 audio profile with original nxplayer and nxrecorder
CLI applications, with 4096-byte main and playback/record thread stacks.
The actual I2S/ES8311 driver is unchanged. Built image435316 bytes, source,
config, maps and receipt archived; SHA256SUMS-xts951 verified.

Original case4.1.126 line4580 uses:

    nxplayer
    device /dev/audio/pcm0p
    playraw /data/1.pcm 1 16 48000
    q

Use actual 48kHz, signed16-bit mono speech PCM to assess pitch/speed and sound.
No playback has run. Supply a genuine file; do not count silence or synthetic
metadata as voice evidence. /data may be a temporary RAM mount for this profile.

Original4.1.125 and4.2.11 request three channels and32 bits. This board path
has a mono ES8311 ADC and current driver16-bit support. Original nxrecorder is
now available for board-mapped mono capture on /dev/audio/pcm0c, but this does
not establish the original three-channel case. Capture real speech, stop,
export and listen to the actual file. Driver is half-duplex: nxlooper is not
enabled and simultaneous record/playback is still unsupported.

The916 receipt now references checkpoint916-flat-audio/nuttx.bin; use951
active-output receipt for the next verified target session. Longrun864 keeps
exclusive UART; no flashing or target operation occurred during this change.

Superseded by963: original rb/sb transfer commands added for speech import
and microphone export.951 remains frozen BUILD ONLY. See checkpoint963-audio-transfer.md.
