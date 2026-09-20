# Original WAV case 4.1.131: temporary composite volume

Preparation only. Do not execute until longrun864 has completed its actual24h
review and the collector has released UART. Keep all original clock cadence
and host-capture deviations. No Wi-Fi provisioning or network streaming.

Candidate987 is independent of frozen985. Qualify968 audio transport,979
duplex, then985 media initialization/playback first. A candidate build or a
host filesystem test is not original audio acceptance. No first-boot guarantee.

## Storage scope

The volume exposes only `/dev/wavvol`: 14 MiB volatile PSRAM followed by the
fixed Flash range `[0xd00000, 0x1000000)`. It never includes the original xTS
scratch interval `[0xc00000, 0xd00000)` or production firmware/AppFS below it.
Boot allocates RAM and registers the device; boot never mounts or formats it.
The original WAV occupies17,473,937 bytes. Current LittleFS host capacity
verification wrote and remounted the intact file with307,200 bytes remaining.
This does not prove target contiguous allocation or decoder memory headroom.
The remaining approximately2 MiB PSRAM must support services and playback.

The volume is temporary: losing RAM destroys the filesystem even though its
Flash portion remains. Do not use it for persistence, reset or endurance tests.
A failed attempt is retained, not automatically formatted or repeated. After
reboot, a previous blank backup must never silently authorize another format.

## First use after longrun

1. Run `backup-media-flash-987.py FRESH_BACKUP_DIR` before any writes to this
   range. This helper invokes esptool and resets the board; it is forbidden
   during864. Both3 MiB reads must match and be completely0xff. Nonblank data
   is preserved and blocks this sequence. The946 backup is not interchangeable.
2. Flash only the matching987 receipt through the matching FLAT profile helper.
   Preserve the image hash and full boot transcript. Require the successful
   `xTS WAV volume: /dev/wavvol` marker with exact Flash offset0xd00000,
   size0x300000 and total17,825,792 bytes. Inspect free memory and failures.
3. With current boot evidence, run:

   ```text
   python3 prepare-media-volume-987.py --receipt build987-flat-media-volume.sha256 --flash-backup FRESH_BACKUP_DIR/manifest.json --boot-evidence CURRENT_BOOT_LOG --output FRESH_MOUNT_EVIDENCE
   ```

   The helper verifies the image, original blank backups, boot marker and UART
   ownership before explicitly formatting only `/dev/wavvol` at `/wav`.
   It consumes the backup authorization before the format command. It cannot
   auto-retry a failed format. The mount is not an audio test result.
4. Prepare `/data` tmpfs, the three exact media985 runtime configuration files
   under `/data/media`, and `/tmp/media-kv` as documented for985. The original
   WAV alone uses `/wav`. Do not stage other audio attachments in this volume.
5. Upload the intact original attachment:

   ```text
   python3 audio-file-transfer.py --upload ORIGINAL_PATH/audio_file.wav --directory /wav --receipt build987-flat-media-volume.sha256 --output FRESH_UPLOAD_EVIDENCE
   python3 audio-file-transfer.py --download audio_file.wav --directory /wav --receipt build987-flat-media-volume.sha256 --output FRESH_READBACK_EVIDENCE
   ```

   Both commands enforce the original length and SHA256. The upload validates
   the local file and target length; only the successful download proves full
   target readback identity. At115200 baud each direction takes at least25.3
   minutes before protocol/Flash overhead; reserve around one hour total.
   The transfer timeout scales to the unchanged original size. No baud change,
   reset or conversion is performed. Preserve heap values before media startup.
6. Start `mediad &` and inspect successful graph/policy/task initialization.
   Use the original mediatool dialogue with only the storage path changed:

   ```text
   mediatool
   open Music
   prepare 0 url /wav/audio_file.wav
   start 0
   close 0
   q
   ```

   Confirm handle0 before using it. Wait for the actual playback to complete
   before close; preserve completion messages, elapsed time, failures and human
   listening observations. The WAV header declares about90.045 seconds of PCM
   at44.1kHz stereo16; the complete attachment includes further original data.
   Do not truncate to the PCM chunk or replace with a generated tone. Disclose
   `/wav` temporary storage and physical mono codec output. A start ACK alone
   is not a PASS. Any memory/transport/decoder failure remains an open defect.

After use, preserve evidence and leave the temporary Flash range untouched
until a deliberate cleanup decision; no automatic erase/restore is prepared.
The original attachment SHA256 is
`20d7c680be243cac559c1d390a324c5b6740dc32471647c91a7c544fb9df5ef7`.
