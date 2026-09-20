#1273 bounded Flash/audio trace

1271 original AAC from Flash had PREPARED/STARTED0 but no completion180.011s;
1272 position query20s returned nothing. Prior1249 same original AAC from TMPFS
completed92.330s. This implicates a Flash/audio interaction, not a proven cause.
1265 failure image/ELF/config and logs are frozen in checkpoint1272-flash-aac-stall.
Original test filesystem was restored and independently verified1266 before
adding only the new audio_file.aac. Do not format this filesystem. The intact
AAC was target-readback SHA verified1270; retain it for the diagnostic run.

1273 enables only a default-off diagnostic flag. Source before traces is saved
in source1272-before-flash-trace. Once audio DMA starts, the first spi_flash_read
emits bounded ROM UART markers from internal code/data:
F=entered; L=operation lock acquired; B=before cache guard; C=cache guard returned;
R=hardware read returned; E=cache restored; M=copy complete; U=unlocked/return.
C/R only bracket the first64-byte chunk; U covers the whole request. No markers
are emitted before audio DMA, so normal file transfers are unaffected. Markers
add timing perturbation: this image is diagnostic, not acceptance. Remove the
traces after localization. No HAL/IDF source was modified.

Fresh boot helper1274 mounts existing Flash without formatting and checks AAC
length; runtime configs1275 and daemon1276 precede playback1277. No automatic
format, file overwrite, or replay after failure is allowed.
