# Original4.1.127 nxlooper on the independent duplex candidate

BUILD ONLY.979 final clean build and host logic review are complete.864 owns the board until its
actual24h evidence review ends. No provisioning or concurrent serial client.

First qualify968 independent capture/playback.979 is a separate opt-in FLAT
single-core candidate, not a replacement for968 or the SMP/MMU final demo.
It intentionally supports48kHz,16-bit,two-slot buffers in512-byte multiples;
original nxlooper default4096-byte buffers remain unchanged. Two slots do not
represent two physical microphones. No original nxlooper source is modified.

After flashing the final979 image using xts-flat-audio-duplex, preserve the boot
log and receipt and confirm both /dev/audio/pcm0p and /dev/audio/pcm0c exist.
The original document uses pcm13c as its example; the board's actual capture
node is pcm0c. Record this mapping explicitly. Run the original interactive
sequence, using the application's own prompt for the inner commands:

```text
nxlooper
device pcm0p
device pcm0c
loopback 2 16 48000
stop
q
```

The source requires SYSTEM_NXLOOPER and preferred-device support, although the
published configuration example only lists SYSTEM_NXRECORDER. The dedicated
profile enables the actual application. Do not issue nxlooper commands at the
NSH prompt or start another serial reader during the session.

Retain the entire session through return to NSH, including asynchronous driver
errors between prompts. The published expectation is commands without errors;
there is no published repetition count or mandatory long duration. Record the
actual elapsed loopback interval rather than adding a new stress requirement.
A prompt by itself does not erase a reported DMA/codec failure. Preserve any
failure and diagnose it before retrying; never silently substitute sequential
cmocka capture/playback for simultaneous nxlooper operation.

No external GPIO fixture or sensor is needed for command execution on the
onboard codec/microphone. Audible end-to-end presentation additionally requires
an appropriate speaker/headphone and actual observation; absence of that
observation must not be represented as verified audio quality. Do not conflate
this with original3-channel/32-bit recording or48/44.1k rate qualification.
