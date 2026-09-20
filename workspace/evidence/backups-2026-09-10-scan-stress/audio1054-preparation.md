# Candidate1054: original44.1k recording with continuous DMA

1038/1049 recordings are not accepted as clean audio. User listening reports
strong electrical noise. In1049, left-channel RMS by4096-byte APB phase peaks
at sample0 (15849 versus roughly900..1260 elsewhere). Original968 stops I2S
and resets/reconfigures RX at every APB; boundary discontinuities are therefore
a concrete transport suspect. Right-slot silence is consistent with mono ADC,
not proof of a second microphone or a missing second channel.

This independent xts-flat-audio-continuous profile inherits979's continuous
RX-master/TX-slave ring DMA and shared ES8311 lifecycle. New default-off
ESP32S31_I2S_DUPLEX_44K selects fixed44100Hz/stereo16, MCLK11289600Hz,
BCLK1411200Hz, MCLK/BCLK8. Codec coefficient, defaults, configure validation,
capability report and queue timeout byte rate all use the same fixed rate.
The original 4096-byte capture buffer, sample contents and test are unchanged.
DMA runs across buffer callbacks until the actual stream stop. No leading
sample is removed or overwritten as a noise workaround.

The new option also explicitly enables left_align, matching the pinned IDF
Philips slot macro for S31 hardware version2. Bit shift remains enabled,
WS width16, normal WS/BCLK polarity, little-endian and MSB-first. Sixteen data
bits equal the slot width; this setting restores the documented default but
is not independently proven to be the cause of the noise. Existing48k979
configuration and968 half-duplex code remain unchanged with the option off.

The codec's physical ADC/DAC are mono. Board pins remain MCLK52/BCLK53/DIN54/
WS55/DOUT56, PA57. The onboard electret mic requires no external fixture.
J9 has no attached speaker, so board speaker listening is not claimed.

Build checks include fixed44.1k codec coefficient presence, source whitespace,
shell syntax, dependency lock, active config and link symbols. Continuous DMA
behavior, initialization and recording quality require actual board evidence.
After flashing the matching receipt, run the unchanged original sequential
record/play test and export intact PCM for boundary statistics and user
listening. Host build alone is not an audio PASS. Frozen968/979 are preserved.
