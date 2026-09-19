# Original mediatool audio sequence (985 preparation)

This is an execution plan, not target evidence. Candidate985 passed its final
clean build on September16 at11:47; the frozen receipt/archive records its identity.
Do not open UART before864 final real24h review and collector release.
First qualify968 codec/I2S and979 continuous transport; preserve all results.
The media profile uses local AF_LOCAL IPC only: no Wi-Fi/IPv4/IPv6 provisioning.

After the matching985 image has been built, archived, flashed and its boot
verified, inspect `mount`, `free`, and the pcm0p/pcm0c device nodes. This FLAT
profile shares its PSRAM user/filesystem heap. Record actual available memory.
Create a fresh disposable `/data` tmpfs only if not already mounted; do not
format Flash or overlay existing files. `/tmp` is already a board tmpfs.

The three runtime files come from
`openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/xts-flat-media/media/`:
`graph.conf`, `criteria.txt`, `settings.pfw`.
Use `audio-file-transfer.py --upload FILE --receipt RECEIPT --output FRESH_DIR`
for each original file. This stages its basename under /data. After checking
successful transfers and source hashes, create `/data/media` and move only
these three staged files there. Preserve exact files in the image evidence
archive. Create `/tmp/media-kv` for the selected file KV backend. Then run
`mediad &`; preserve initialization output and `ps` before starting mediatool.
A failed daemon/graph initialization is a failure to resolve, not a reason to
ignore diagnostics or continue with a substitute audio command.

Transfer each MP3/AAC/Opus attachment separately at its unchanged original
basename. Source paths, sizes and hashes are in
`audio-original-resources-984.json`. Check live RAM can hold the full attachment
plus decoder, graph, task and filesystem allocations; do not stage all media
files simultaneously. Upload checks length. For byte-for-byte evidence,
download it with original sb through the same helper and compare SHA256 to the
source. Close each host serial handle before another host command runner.

For each original case, save the complete console dialogue:

```
mediatool
open Music
prepare 0 url /data/audio_file.mp3
start 0
close 0
q
```

Use `.aac` for4.1.130, `.opus` for4.1.129 and `.mp3` for4.1.128. Confirm the
returned handle is0 before using0 in later commands. Preserve initialization,
prepare, playback/completion, close and exit messages, elapsed time and actual
listening observations. Play the intact original resource; do not count a
start acknowledgement as successful music playback. MP4 demux is needed for
the `.aac` attachment. The graph uses the existing route-aware asubgraph with an internal aresample
filter; the native <subgraph> section initializes it before policy/player use.
A plain aresample between the main graph endpoints is incompatible with this
checkout's route negotiation and must not be substituted. The graph converts
decoded audio to the actual fixed
48kHz/S16LE/two-slot codec transport; the physical ADC/DAC remains mono.

The original4.1.131 WAV file17473937bytes cannot be staged whole in the existing
16MiB memory or16MiB Flash. It remains pending suitable storage/streaming and
any necessary disclosure of changed setup. No cropping/transcoding/substitute
fixture is accepted as the untouched attachment. No additional display/video
or network feature is required by this board-only preparation.

After saving a case's evidence, remove only its own staged media file if space
is needed for the next case. Leave unrelated files, audio captures and Flash
storage evidence intact. No automatic retry, reboot or failure masking.
