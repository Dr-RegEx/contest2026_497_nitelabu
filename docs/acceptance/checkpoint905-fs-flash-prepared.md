# Persistent filesystem category candidate 905

Full offline build PASS, kernel418756 bytes. TARGET NOT RUN. Separate
xts-flat-category-fs-flash extends892 and registers /dev/xtsflash at fixed
0xc00000..0xcfffff, with LittleFS and original power_off_test01/02/03.
All three entry points/configs are verified. Original assertions/workloads
are unchanged. This is a build candidate, not persistence/power-loss evidence.

Startup never mounts/formats/writes the test MTD. First verify and double-backup
blank scratch via backup-flash-scratch.py, then run common865 raw block tests.
Only afterwards prepare isolated LittleFS /data explicitly. Never format or
fill production/apps. Remount without forceformat after each reboot/power
cut; formatting would destroy the evidence that power-loss recovery must read.

Original4.1.4/5/6 each require actual power removal and3 repetitions. They
cannot be passed by software reboot, UART reset or RAM filesystems. Preserve
raw UART before/after power cut, test filenames, space/data results, and actual
supply-removal method. Prepare each original case's fresh state deliberately;
for power_off_test01 the existing-file path validates and exits rather than
starting another write loop. A next independent round needs fresh test state
after recording the previous successful recovery. See original sources and
published body before each physical run, no automatic hidden retries.

Flash writes from arbitrary PSRAM-resident FLAT task stacks are not yet a
validated port guarantee; common865 is the first hardware gate. If that exposes
a cache-off fault, repair it before categoryFS/KVDB Flash writes. No speculative
pass or extra Flash write has been performed during864.

September16 update: candidate949 replaces905 for target execution with the PSRAM-stack fix described in checkpoint946-flash-psram-stack.md. Use build949-flat-category-fs-flash.sha256 only after successful build receipt and archive verification. The old905 receipt points to its frozen image and must not be supplied to the active-output flash helper. Historical commands above retain the original preparation record.
