# Existing IDF audio fixture (host preparation only)

hi-idf-source.wav is copied from the locked local IDF DAC audio example.
hi-idf-mono16-48k.pcm contains exactly its PCM sample bytes, with only the
WAV container removed. No download, synthesis, resampling or source edit.
The source/example remains under its existing license; retain this origin.

Import through original rb using the single UART owner only after longrun864
and any required same-boot clock extension have ended, and audio968 is booted.
Verify191648-byte file size, then select48000Hz,16-bit,mono explicitly in
nxplayer/raw playback. The file does not use the default44100Hz format.
Keep the host WAV as a comparison reference for actual speaker listening.
Metadata/hash verification is not listening validation or target audio PASS.
Do not count this file alone as proof of microphone capture.
