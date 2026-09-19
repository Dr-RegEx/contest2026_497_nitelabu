# 1032 separate 3 MiB filesystem candidate — PREPARED, NOT BUILT

Profile: `xts-flat-category-fs-large`, inheriting the corrected1023 fs-name
profile (and976 performance profile). New default-off
`CONFIG_ESP32S31_XTS_FLASH_LARGE` changes only the xTS device's fixed Flash
partition from0xc00000+0x100000 to0xd00000+0x300000. Original/default profiles
retain the1MiB region unchanged. No original xTS source or workload is changed.
The same `/dev/xtsflash` name is retained; its boot marker must show the new
exact offset/size. Registration only: no mount, erase, or format at startup.

Configuration also retains recovery prerequisites:32KiB testcase stack,
BOARDCTL_RESET, reset-on-assert2, and NSH_LINELEN128 for complete published
command lines. These enable original800000-byte random read/write,1MB
sequential workloads and documented crash/recovery runs. A3MiB volume provides
more room than the failed1MiB case; real workload success remains unproven.

## Exact-range ownership and preservation

The3MiB region overlaps candidate987's WAV volume backing Flash. Kconfig
prevents selecting large scratch with the WAV volume, and the board source
also emits a build error if both are forced. This exclusion does not prove
that old physical content is blank. The initial FS use requires two independent
backups of the entire0xd00000..0x1000000 range, equal SHA256, exact3MiB size,
and an all-FF check. The9901MiB backup is insufficient for this range.

Never erase or format0xc00000..0xd00000; KV and prior FS evidence remain there.
Before first formatting, verify the new image receipt/config and new scratch
boot marker. Existing1012 helper hardcodes948 and1MiB and must not be reused
or loosened. Existing mount-fs-scratch.py also validates the1MiB marker, and
has intentionally not been changed until1032 has an actual receipt and a
range-specific guarded execution plan. No script currently grants new-range
formatting through this preparation.

After FS testing, preserve full transcripts, result files and a raw image of
the3MiB region before any reset of its contents. Before987 may reuse this
region, explicitly restore the verified original all-FF state, confined to
this range, and read back the full3MiB to verify equality/hash. Do not mount
an FS-test volume as987's RAM+Flash filesystem: its geometry and ownership
are different. No automatic erase/restore is implemented here.

## Deferred build

Only when the shared build slot is available, the sole assigned builder runs:

```
S31_FLAT_PROFILE=xts-flat-category-fs-large bash backups/2026-09-10-scan-stress/build-xts-flat.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build1032-flat-category-fs-large.sha256
```

Build/flash helper allowlists and configuration checks are prepared. Shell
syntax and source whitespace checks passed. No1032 build/receipt, UART,
backup, formatting, Flash operation or test was performed by this preparation.

## Build and guarded first mount prepared

1032 clean build now PASS:419492bytes, SHA256
`7b0e0dd869e157a1620297412caba0d99b5e6b068fff0e3a00bf05ceb7ced47a`.
Receipt `build1032-flat-category-fs-large.sha256`, archive
`checkpoint1032-fs-large/`. No target operation by this preparation.

Reuse `backup-media-flash-987.py <fresh-directory>` for the exact physical
3MiB double read: despite its media-oriented name, its offset/size are the
same physical interval. It does not write; sole UART owner must schedule it.
Do not substitute the9901MiB backup. Flash1032 using the existing helper and
new profile, then the sole UART owner may run:

```
python run1032-fs-large-first-mount.py --flash-backup <fresh-directory>/manifest.json --output <fresh-mount-directory>
```

The new dedicated runner validates the exact1032 receipt/config, two distinct
full3MiB all-FF backups and their hashes, and UART availability. It claims a
fixed exclusive attempt token and consumes the backup using the same
`used-by-media-volume-987.json` marker recognized by the media formatter,
preventing the old pre-FS backup from later authorizing987 formatting. It
performs one reset, checks the3MiB boot marker, refuses existing data/apps
mounts, and forceformats only/dev/xtsflash onto/data once. It never registers
or accesses the preserved1MiB partition. Mount/df/free output is retained as
current-boot evidence. Failure never removes tokens or retries formatting.

Syntax/help and offline preflight checks PASS. The latter used the real1032
receipt/config and synthetic temporary backup files, including consumption
rejection; they are not physical blank-state evidence. Post-test evidence
preservation and exact-range blank restoration before987 remain required.
