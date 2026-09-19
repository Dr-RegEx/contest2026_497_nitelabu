# Media helper guards 987 — host only

The actual Python helper sources were imported/executed with substituted
UART contexts, subprocess calls and a blocked UART open boundary. No real
serial access, esptool operation, reset, Flash write, formatting or file
transfer occurred. All temporary fixtures were host-only synthetic data.

Run from the project root:

```sh
s31-reference/.venv-nuttx/bin/python backups/2026-09-10-scan-stress/host-media-helpers-987/check.py
```

`result.json` records 17 successful guard/path checks. They cover the wrong
1 MiB manifest; declared and actual nonblank backup data; a consumed manifest;
wrong profile; busy UART; existing volume mount; the actual ESPRESSIF_WIFI
symbol; two read-only backups of exactly 0xd00000..0x1000000; backup refusal
while busy; preservation/refusal of nonblank backups; the intact original
WAV preflight and SHA-256; default /data compatibility; malformed WAV and
wrong-profile rejection; and the actual dynamic transfer-deadline expression.

The original WAV is 17,473,937 bytes with SHA-256
20d7c680be243cac559c1d390a324c5b6740dc32471647c91a7c544fb9df5ef7.
Its computed transfer allowance is 3213.67 seconds, exceeding the ideal
115200 baud 8N1 wire duration; small files retain the 600-second allowance.
This calculation does not establish actual Flash throughput or transfer
success. Positive preflight checks stop before the hardware boundary.

One real issue found during review was the obsolete ESP32S31_WIFI guard
symbol. The root agent corrected it to ESPRESSIF_WIFI before this successful
run. The helper scripts were not edited by this audit. Image stand-ins do
not establish the board is running the referenced candidate. Upload results
record the local source hash; actual target integrity still requires the
planned download/hash comparison. No original audio PASS is claimed.
