# xTS 1.2.3 / 1.2.4 resource footprint — firmware730

Evidence: logs/xts736-footprint.log. Original `free` and `df -h` run with
NSH restricted first to CPU0 and then CPU1. `/proc/6/status` confirms
CPU000(mask1) / CPU001(mask2); original mask3 restored afterward.
Test735 was a host parser failure on the full `/system/bin/init` name,
before any affinity change; its log is retained, not counted as a board fault.

| Shared pool | CPU0 snapshot bytes | CPU1 snapshot bytes |
|---|---:|---:|
| Internal kernel heap total | 304828 | 304828 |
| Internal kernel heap used | 78524 | 78588 |
| Internal kernel heap free | 226304 | 226240 |
| PSRAM page pool total | 16777216 | 16777216 |
| PSRAM page pool used | 696320 | 696320 |
| PSRAM page pool free | 16080896 | 16080896 |

This is one SMP kernel with shared physical RAM/Flash, not two independent
OS images. Do not sum the per-CPU snapshots. Small heap differences reflect
the measurement commands. `free` describes allocatable pools, not total
silicon SRAM: static kernel sections, reserved stacks and mappings also use
internal memory. Kernel ELF is archived in firmware730-xts-io.tar.gz.

## Physical Flash allocation

16MiB flash selected and validated by the guarded flash procedure. Boundaries
below follow firmware730 configuration and the board bringup partitioning.
End addresses are exclusive; allocated regions are not all occupied bytes.

| Start | End | Purpose / measured occupancy |
|---|---|---|
| 0x000000 | 0x002000 | Outside simple-boot kernel image; preserved |
| 0x002000 | 0x200000 | Kernel slot; nuttx.bin1052084bytes |
| 0x200000 | 0x500000 | Read-only seed AppFS slot3MiB; image1804288bytes |
| 0x500000 | 0xc00000 | Writable LittleFS `/apps`,7MiB; df used8KiB |
| 0xc00000 | 0x1000000 | Outside configured storage MTD; preserved |

Both CPUs report the same `/system/bin` ROMFS (1761KiB) and `/apps`
LittleFS (7MiB). `/proc` is virtual; `/tmp` is RAM and must not be counted as
Flash. ROMFS `df` measures the mounted image, not unused reserved-slot space.
Firmware archive, config and logs preserve the precise revision snapshot.

Result: documented RAM and Flash footprint collection complete on both CPUs.
No capacity/performance threshold is specified by these two checklist items.
