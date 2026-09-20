# Candidate1101: original16k mono capture

BUILD ONLY. This profile supports the original4.2.13 capture parameters
`-a1 -s16000 -c1 -b16` without changing the original application. Frozen1057
and its successful1059 speech evidence remain intact.1087 listening is pending.

Dedicated profile xts-flat-audio-mono enables default-off
ESP32S31_I2S_CAPTURE_16K_MONO, mutually exclusive with the44.1k option.
It uses the existing continuous ring DMA, fixed16000Hz,16bits, native hardware
RX mono/left slot. The physical I2S frame still contains two16-bit slots:
BCLK512000Hz, MCLK4096000Hz, divider8. RX DMA contains one16-bit ADC sample per
frame, not two slots with one discarded by software. No resampling, sample
filter, bit rewrite or boundary-sample deletion is introduced.

The pinned S31 HAL RX setup calls rx_enable_mono_mode and rx_select_std_slot;
the LL sets rx_mono/rx_mono_fst_vld and active mask1 while preserving two wire
slots. Philips shift and left alignment are enabled. Codec reset/configuration
and capability reporting use16000Hz/one input channel, with an existing exact
4096000/16000 coefficient. RX queue budgets use32000bytes/second. Start/STOP,
request callbacks and ring ownership retain the1057 continuous implementation.

Playback remains unsupported in this profile: output configure returns EINVAL
and query reports no output capability. The inactive TX clock setup remains a
stereo slave to the RX master; it does not fabricate a second microphone.
The existing pcm0p node is registered but cannot configure a playback stream.
Use pcm0c for capture. This candidate does not replace1057 stereo44.1k playback
or97948k nxlooper. Native16k mono nxrecorder can use the same capture endpoint
with explicit16k/mono16 parameters, but no new test acceptance is claimed.

Validation: clean independent firmware build, selected config/link checks,
codec coefficient and pinned HAL slot-path review, F0 dependency verification,
source whitespace/shell syntax checks. The1057 image receipt still verifies.
Hardware clock/rate, actual ADC mono packing, exported PCM duration/content and
human speech listening must be checked on board. No UART/reset/flash or
provisioning was performed during this host preparation.
