# Candidate 987: intact original-WAV media volume

Status: host preparation only; no board flash, UART, provisioning, formatting,
WAV upload, target playback or listening PASS is claimed. Frozen985 is intact.
Profile `xts-flat-media-volume` inherits985 media/codec configuration and946
Flash dispatch from PSRAM task stacks to the internal 4096-byte LPWORK stack.

Default-off ESP32S31_XTS_MEDIA_VOLUME reserves one contiguous 14 MiB buffer
at the start of board bringup, validates both endpoints as mapped PSRAM and
initializes only that volatile RAM to 0xff. It creates RAMMTD with 64-byte
blocks/4096-byte erase units and an exact Flash partition at 0xd00000 of
0x300000 bytes. The old 0xc00000..0xcfffff xTS range is untouched. Geometry is
validated before one MTD device, /dev/wavvol, is registered. Registration does
not mount, format, erase, or write Flash. There are no block/BCH aliases.

The dedicated wrapper concatenates RAM then Flash. Entire ranges are checked
before dispatch, including negative offsets and SIZE_MAX counts, so invalid
cross-boundary writes cannot modify a valid prefix. Valid reads/writes cross
14 MiB with correct offset/buffer progression, return accumulated short counts,
and propagate errors when no prefix succeeded. Erase requires RAM's exact OK
return and Flash's exact erased-block count; negative, zero/short Flash errors
are not accepted as success. Only geometry ioctl is provided; BULKERASE and
other parent controls return ENOTTY without forwarding. No parent whole-chip
erase can be reached through ioctl. A cross-boundary Flash I/O failure can still
leave a valid RAM prefix modified; no atomicity or persistence is promised.

Initialization is attempted once per boot. A failed allocation/geometry/device
registration returns an explicit error to board bringup and frees unregistered
RAM. A Flash partition object may remain allocated on a failed boot because
this tree has no public partition destroy API; no device is published on that
failure. No fallback truncates the buffer or original WAV. The volume is
volatile across reset: every fresh boot needs a guarded new format and upload.

Total capacity is 17,825,792 bytes. The untouched original WAV is 17,473,937
bytes. Actual current LittleFS host format/write/unmount/remount/readback of
this whole resource passed SHA verification, using4277 of4352 erase blocks
(307,200 bytes free). This demonstrates filesystem capacity, not target heap
sufficiency. Approximately2 MiB PSRAM remains before system, media, decoder,
task and filesystem overhead; target free memory/boot/playback must be checked.

Runtime graph/criteria/settings stay under /data/media in TMPFS. The WAV alone
is stored at /wav/audio_file.wav, with /dev/wavvol mounted explicitly at /wav.
Use the frozen double-backup/format/upload guards and media-target-sequence-987.md.
Never reuse this device as a persistent filesystem across a power cycle.
