# Audio968: original44.1kHz support, BUILD ONLY

The S31 I2S controller now selects11.2896MHz MCLK for44100Hz; all previous
rates still select12.288MHz. The existing ES8311 coefficient table includes
this new pair. A default-off ES8311_RATE_DEPENDENT_MCLK option configures the
I2S rate before querying MCLK. Only the S31 audio profile enables it here.
Half-duplex transport and original test programs are unchanged.

Complete offline build, dependency lock verification and source whitespace
checks passed. Image442116 bytes. No target flash, audio recording, playback
or waveform measurement was performed.963 is frozen as fallback.

After actual24h clock acceptance and any necessary same-boot extension:
use the existing flash-xts-flat.sh with S31_FLAT_PROFILE=xts-flat-audio and
build968-flat-audio.sha256. Actual board microphone/speaker qualification and
original cases remain pending. Never flash during longrun864.

Original4.2.15: cmocka_driver_audio -a 3 -p /data/xts-sequential.pcm
retains the original44100Hz, two-slot,16bit,10-second defaults. Verify a fresh
file path and enough live RAM before execution (nominal1764000 bytes plus
buffer/allocation overhead). Preserve capture/playback logs and real audio.
Two slots do not mean two physical microphones on the mono codec.
Also qualify the existing48kHz path; this document does not claim either passed.
Full-duplex nxlooper and3-channel/32-bit cases remain unsupported.
