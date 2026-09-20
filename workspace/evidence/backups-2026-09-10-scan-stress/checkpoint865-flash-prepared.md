# Board Flash block test prepared — build only

The user requested continuous work and a progress update after every completed
adaptation/test. Longrun864 keeps exclusive UART; this work was entirely offline.

`xts-flat-flash` inherits the verified FLAT RTC/memory profile and adds the
existing SPI Flash MTD driver. Board initialization registers `/dev/xtsflash`
at offset0xc00000, size0x100000. This is outside the established kernel/AppFS
and writable LittleFS range ending at0xc00000. No boot-time erase, write or
format occurs. The partition uses the existing bounded MTD implementation.

Build865 completed successfully. Original cmocka block and RTC entry points
and the new partition registration function are present in the linked image.
Existing HAL/rwbuffer warnings remain. Source whitespace and host script
syntax checks pass. **No hardware test has run and no new xTS PASS is claimed.**

After longrun864 completes and releases UART, from the workspace root:

```sh
python3 backups/2026-09-10-scan-stress/backup-flash-scratch.py backups/2026-09-10-scan-stress/flash-scratch-before-test
S31_FLAT_PROFILE=xts-flat-flash bash backups/2026-09-10-scan-stress/flash-xts-flat.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build865-flat-flash.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-flat.py flashblock --flash-backup backups/2026-09-10-scan-stress/flash-scratch-before-test/manifest.json
```

Capture each command in a fresh numbered log and confirm its terminal result
before the next command. The backup command reads the fixed range twice and
verifies equality/size/digests; nonblank data stops the workflow and is retained.
The runner revalidates both blank backups, checks the boot partition marker,
then invokes unchanged `cmocka_driver_block -m /dev/xtsflash`. All three cases
must pass, including cache-write without the insufficient-size early exit.
The flash helper writes only the kernel slot; it does not overwrite AppFS.

`firmware865-flat-flash-build-only.tar.gz` and `SHA256SUMS-xts865` preserve
this compiled candidate. Basic board boot, actual Flash cache interactions,
and original read/write results still require target verification.

Read-only review September15 while864 runs: existing S31 spi_flash_read/write
already copy through a stack buffer with caches restored when accessing caller
memory. The FLAT allocator is best-fit across internal SRAM and PSRAM; task
stacks are not categorically restricted to SRAM. Thus a PSRAM-resident calling
stack is an unresolved limitation of generic FLAT Flash access, not covered by
the buffer copy alone. Run the isolated original case from a fresh boot as
prepared and preserve any failure; do not infer support for arbitrarily placed
PSRAM stacks. No source or candidate image changed during this review.

September16 update: candidate946 replaces865 for target execution with the PSRAM-stack fix described in checkpoint946-flash-psram-stack.md. Use build946-flat-flash.sha256 only after successful build receipt and archive verification. The old865 receipt points to its frozen image and must not be supplied to the active-output flash helper. Historical commands above retain the original preparation record.
