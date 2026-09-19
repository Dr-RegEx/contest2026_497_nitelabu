# Flash PSRAM stack dispatch — candidate946, BUILD ONLY

The FLAT heap includes PSRAM and generic task stacks use that heap. Existing
S31 Flash transactions disable the external-memory cache. Buffer copies after
cache restoration do not protect the calling stack itself.

ESP32S31_SPIFLASH_PSRAM_STACK now defaults on for FLAT + PSRAM heap + MTD +
LPWORK. All five common MTD entry points dispatch read/write/erase calls from
non-internal stacks to LPWORK. Internal-stack calls stay direct; the worker
checks its own stack before any cache-off operation. Existing S31 transfer
buffer and Flash guard behavior is retained. Each caller owns its work item;
cancellation is disabled until completion so work arguments and data buffers
cannot be released by cancellation while the callback is using them.

946 built successfully, 330432 bytes. Linked LPWORK stack is internal SRAM
0x2f00c450 (4096 bytes); dispatch/worker/execute symbols are present. Source
whitespace and helper syntax checks passed. No target Flash operation occurred.
The original865 binary/config/maps are frozen in checkpoint865-flat-flash;
its receipt now points there. Latest active-output receipt is946.

All subsequent Flash/KVDB/filesystem candidates need the same source update.
Build and flash helpers require the dispatcher when XTS Flash and PSRAM heap
are both enabled. The fixed scratch range and double blank backup requirement
remain 0xc00000..0xcfffff. Never touch the UART during longrun864.

Current execution sequence after longrun acceptance and release of UART:

```sh
python3 backups/2026-09-10-scan-stress/backup-flash-scratch.py backups/2026-09-10-scan-stress/flash-scratch-before-test
S31_FLAT_PROFILE=xts-flat-flash bash backups/2026-09-10-scan-stress/flash-xts-flat.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build946-flat-flash.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-flat.py flashblock --receipt backups/2026-09-10-scan-stress/build946-flat-flash.sha256 --flash-backup backups/2026-09-10-scan-stress/flash-scratch-before-test/manifest.json
```

Capture each operation separately in a fresh numbered log. The runner now
requires the Flash image receipt and PSRAM-stack config, in addition to the
existing verified blank scratch backups. UART ownership is checked before
opening the port. No additional device test was added or executed.
