# Original raw Flash throughput candidate909

Full offline build PASS; TARGET NOT RUN. New xts-flat-flash-raw profile
inherits common865, adds ESP32S31_XTS_FLASH_RAW and original dd statistics.
The existing verified-geometry1MiB partition at0xc00000 is exposed through
FTL /dev/xtsblock and BCH /dev/xtsraw. Both use this tree's O_RDWR API.
Normal865/895/905/907 profiles leave the new option off. There is no scratch
filesystem mount or erase/write during device registration. Source review
covered FTL allocation/open and BCH setup; whitespace check passed.

908 was rejected before hardware because the older BCH readonly-boolean API
was initially assumed. Local BCH takes open flags; candidate909 corrects this.
The908 binary, source and unchanged digest are preserved under checkpoint908-rejected.

Execute ONLY after864 finishes, double blank backup of0xc00000..0xcfffff is
verified, and original865 block tests pass. Schedule before LittleFS
formatting/KVDB/FS evidence to avoid destroying their data. Inspect mount,
boot offset/size marker and /dev/xtsraw. Refuse if any scratch filesystem is
mounted or retained evidence occupies this range.

Original5.1.13 permits explicit blocksize/count. Use4096x64=256KiB within the
1MiB range and record that choice; then run separately:

```text
dd if=/dev/zero of=/dev/xtsraw bs=4096 count=64
dd if=/dev/xtsraw of=/dev/null bs=4096 count=64
```

Retain both measured rates, no command errors/crashes. This is original raw
throughput measurement, not a substitute for common block integrity tests.
Do not invent a numerical speed threshold. Never use /apps or other devices.
No command above has been run. The one-board864 longrun remains untouched.

September16 update: candidate947 replaces909 for target execution with the PSRAM-stack fix described in checkpoint946-flash-psram-stack.md. Use build947-flat-flash-raw.sha256 only after successful build receipt and archive verification. The old909 receipt points to its frozen image and must not be supplied to the active-output flash helper. Historical commands above retain the original preparation record.

September16 05:09 preflight: the unchanged nsh_ddcmd.c reports
`262144bytes copied, <elapsed> usec, <rate> KB/s` for each4096x64 command.
Require the full byte count, positive elapsed time, a reported speed and no
NSH/I/O/fault errors for both directions. The published case has no numerical
speed threshold. Use947 after946 succeeds and before filesystem formatting;
there is no need to add extra write loops or an invented stress workload.
