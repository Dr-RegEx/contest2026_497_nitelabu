# Current WAV preparation after media fixes

1254 is a new isolated BUILD PASS candidate, not flashed. It inherits the
validated1243 media fixes and uses the unchanged original17,473,937-byte WAV.
Keep the active MP3 test on1243 until its actual completion/cleanup.

The later xTS large filesystem owns0xd00000..0xffffff. It must be preserved;
987 blank-only instructions predate its use. backup1255-xts-large.py preserves
two matching full3MiB reads, records the actual blank/nonblank state, and does
not authorize formatting. Run only after UART release; esptool resets the board.

scratch1256-wav-maintenance.py provides fixed-range erase and restoration from
that specific backup. Before any erase, retain both verified originals and the
intent record; no full-chip erase or firmware/AppFS mutation is permitted.
After deliberate scratch cleanup, use the unchanged backup-media-flash-987.py
for fresh actual blank reads, then flash1254, verify its boot volume marker,
and use prepare-media-volume-987.py with this fresh blank evidence. Never edit
an occupied backup's blank flag to satisfy the original guard.

Prepare the existing three runtime configs in /data/media, /tmp/media-kv,
and /wav as documented. Upload and read back the entire original WAV at the
fixed baud, then execute the original mediatool sequence. /wav is volatile
storage; capture logs do not prove persistence or audible quality.

After WAV evidence is preserved and its UART workload ends, deliberately
restore the original3MiB xTS bytes through scratch1256's restore action and
require its independent full readback to match. Preserve all operation logs.
If any operation fails, retain its intent/results and diagnose before any new
attempt; do not automatically reformat or discard the saved filesystem.

No scratch erase/restore, WAV format, or1254 target boot has occurred merely
by writing these preparation notes.
