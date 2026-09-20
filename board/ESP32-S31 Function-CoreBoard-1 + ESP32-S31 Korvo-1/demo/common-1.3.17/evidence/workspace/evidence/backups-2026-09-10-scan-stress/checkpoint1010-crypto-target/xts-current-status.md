# S31 xTS execution status — 2026-09-16

**新增1010：1002离线配对镜像上八个原始Crypto应用全部PASS，SHA/HMAC真实计数与status=0日志正常，支持的硬件AES/SHA/ECC路径验证完成；3DES/MD5/CRC32软件实现。通用正式29/35，品类3项。看门狗完整序列仍在定位串口输入阻塞。**

**新增1006：品类ROMFS4.1.2、FATUTF8 4.1.3原始用例PASS，品类累计3项。看门狗1004模式0/1真硬复位通过，模式2命令传输未完成，完整序列仍待验证。Crypto1001八个原始应用功能断言通过，SHA/HMAC日志格式修复中。通用暂28/35。**

**新增上板结果（992/994）：原始1.3.5 Flash块测试3/3通过（5.337秒）；品类5.1.13原始dd各256KiB读写通过，写275KiB/s、读2560KiB/s，前后内存稳定。隔离区0xc00000的两份初始空白备份一致。通用28/35，品类正式PASS 1；接着执行看门狗。**

**最新验收口径确认（989）：按用户要求，不额外要求连续日志或精确到秒的采样间隔。1.3.14四次阶段检查误差均≤2秒，最终实际24小时5.07秒、误差−1.684～−0.663秒，项目验收记PASS；前两次提前采样的实际时刻及日志缺口保留。通用更新为27/35，品类PASS 0；988原始证据与先前判断不改写。**

**16:43：864长测完成最终采样并正常退出，串口已释放。实际24小时5.07秒，最终板时相对PC误差−1.684～−0.663秒，满足24h/≤2s数值指标。前两次采样提前及06:33至08:12记录缺口保留，严格1.3.14暂不记PASS。每10分钟主机监测已跟踪到结束；正式通用26/35、品类PASS 0不变。最终证据988。**

**12:13：987完整原WAV候选最终干净构建通过（1,671,852字节）；17MiB临时卷原附件完整读回SHA一致、实际组合驱动边界与17项主机脚本保护检查通过。备用Flash尚未写入，连续PSRAM分配、真实传输/播放仍待上板。可并行的本轮离线准备已补齐，后续按长测收尾后的上板队列推进；不配网。通用26/35、品类正式PASS 0不变。**

**12:07：987完整原WAV临时卷准备进行中：现有LittleFS主机实测17MiB卷可完整写入17,473,937字节原附件，重挂读回SHA256一致，余300KiB。组合MTD/profile已实现，边界回归与构建尚在进行；备用Flash尚未写入，实板播放仍未验证。986原LZF压缩容量证据已归档。长测采集继续，通用26/35、品类正式PASS 0不变。**

**11:47：985原始mediatool音频候选最终干净构建通过，主机PCM能力/子图必要回归通过，未上板。MP3/AAC/Opus附件已核对；WAV原文件超单个16MiB存储容量，限制保留。长测仍在运行、不配网；通用26/35、品类正式PASS 0不变。**

**10:43：原定18小时采样已完成，实际64800.12秒，板/PC误差区间−0.730～+0.290秒；证据982已归档。24小时用例仍在运行，最终约16:43复核；通用26/35、品类PASS 0不变。**

**08:14 恢复取证：原showinfo任务PID7仍在、资源值一致、板时与PC相符；用户确认开发板持续供电。已恢复同次启动的采集，保留06:33至08:12日志缺口，预计16:43达到实际24h；尚未判定24h通过。恢复记录xts864-recovery.json。**

**08:10 中断更正：WSL于08:05重新启动，原864采集进程已不存在；最后串口日志06:33。12h待机归档PASS保留，24h时间一致性未完成，原JSON的RUNNING已过期。USB已重新挂接，正在核查板上连续性，尚未复位/刷机。**

Primary goal: applicable published xTS cases and actual usability. Submission
is September 19 (user confirmed September 15). Aim to finish board validation
and deliver fixture firmware September 16 evening; reserve September 17 for
physical peripheral tests and September 18 for fixes, retests and demo evidence. Existing diagnostics are supporting evidence only. No new
boundary diagnostics unless needed to unblock a standard case or avoid major
data/boot risk. Failed cases remain required and visible.

Authoritative case source: openvela-dev/docs/zh-cn/test_dev_guide/openvela_xts_test_cases.md.

Category coverage is tracked separately in `xts-category-status.md`:161 original
heading instances,29 with confirmed5GHz hardware mismatch, remaining132 requiring
product/fixture applicability and individual acceptance. Category cases are not
included in29/35 below. Storage/KVDB/OTA/NetApp detail: category-storage-audit.md.
No complete category acceptance or overall completion percentage is claimed.

Common checklist has35 headings:29 accepted against target evidence
(82.9% common-checklist coverage, NOT whole-project completion).
Case1.3.14 uses the clarified project acceptance decision989 and retains
actual early sampling times; no exact-six-hour-spacing claim is made.
Twenty are standard functional commands below, plus flash/NSH case1.3.1
and reboot cases1.2.1/2.1.4 plus footprint1.2.3/1.2.4, standby3.1.1
and time consistency1.3.14 under decision989, plus Flash block1.3.5 and Crypto1.3.17 (1010).

Current common-checklist allocation (35 total):

| State | Headings | Scope |
|---|---:|---|
| Target/project acceptance PASS |29| Evidence below;1.3.14 clarified acceptance989 |
| Target validation / fixes in progress |1|Watchdog868 failed996; corrected999 modes0/1 pass, serial delivery blocks full sequence. |
| Statistical output incomplete |1|Original10-stream RNG786; supplemental100-stream808 retained |
| Physical cooperation deferred |4|Reset button, cold-power timing, GPIO jumper, BMI160 |


| Common case | Command | Current result / evidence |
|---|---|---|
| 1.1.1 memory | cmocka_mm_test | PASS on686: all8/8subcases,3.630s, xts689-mm.log; earlier680 PASS also retained. |
| 1.1.2 scheduler | cmocka_sched_test | PASS on686: all9/9, xts688-sched.log. Earlier681/682 fatal and685 ENOMEM retained. Upstream selects9pthread cases in BUILD_KERNEL,7task cases are BUILD_FLAT-only. |
| 1.1.3 syscalls | cmocka_syscall_test | PASS on704:83/83,15.656s,normal NSH return,no assertion (xts709-syscall.log). Earlier692/698/702 failures retained. |
| 1.1.4 ostest | ostest | PASS on771/775,52.081s: full original configured loop reaches user_main:Exiting and status0, no ERROR. Fix retains recipient for syscall-deferred signals instead of allowing another thread to consume shared pending entry. Earlier764premature exit retained. FLAT-only spinlock source condition matches original entry point. |
| 1.1.5 getprime | getprime | PASS on690,1040ms,xts693-getprime.log |
| 1.1.6 heaptest | mm | PASS839 on837: original complete allocation/reallocation/alignment workload, TEST COMPLETE, no failed/skipped allocations,6.758s. Separate FLAT profile; full16MiB PSRAM included. |
| 1.1.7 scanf | scanftest | PASS on690,164OK/0FAILED including#25,xts694-scanftest.log |
| 1.1.8 C | hello | PASS on690,xts695-hello.log |
| 1.1.9 C++ hello | helloxx | PASS on716,all3dynamic/stack/static instances,xts718-helloxx.log |
| 1.1.10 popen | popen | PASS on730, Calling pclose() and clean shell return,733. Current source requires POSIX timers enabled, contrary to old checklist config snippet. |
| 1.1.11 pipe | pipe | PASS on730,FIFO/transfer/interlock/redirection complete,33.542s,732. Source removes its own FIFO paths; doc's duplicate rm is obsolete and not executed blindly. |
| 1.1.12 MD5 | md5_test -f /etc/1.txt -c 100 | PASS on730,100 identical correct hashes,2.339s,734. Fixture created only in fresh volatile /etc tmpfs, no persistent file changed. |
| 1.1.13 C++ functions | cxxtest | PASS797 on794,0.204s:vector/map/C++17/RTTI plus Catch Exception:runtime error, clean NSH return.790/793 capture libgcc unwinder abort and zero EH header:orphan sections cause32-byte loaded offset. RISC-V linker groups small-rodata/exception tables/small-data;795 layout goes from180mismatches to0. Explicit EH registration/terminator retained; temporary diagnostics removed. No original test edits. |
| 1.2.1 / 2.1.4 reboot | reboot10times | PASS on724:10/10 complete NSH/SMP startups, mean0.548s <=6s, no abnormal boot logs (726). Host/UART timing includes overhead; not cold-power-cycle evidence. Board reset fix committed fd5f113e1a0. |
| 1.2.2 reset-button startup | physical reset button | NEEDS button/reset-path evidence. The published step explicitly says reset button despite the Cold Boot heading; do not impose power cycling on this case. |
| 2.1.3 cold boot timing | actual power cycles / timing | NEEDS actual power-cycle evidence for10cycles; UART RTS reset is not accepted as cold-power-cycle timing. |
| 1.2.3 / 1.2.4 footprint | free; df -h on each CPU | Complete on730,736log confirms CPU0(mask1)/CPU1(mask2), mask3 restored. Physical Flash map and shared-memory interpretation in xts736-footprint.md. |
| 1.3.1 flash | existing guarded flash procedure | Prior flash668 + demo669 establish flash/hash/NSH; not a substitute for other cases |
| 1.3.2 RAM file R/W | fstest -n 10 -m /tmp | PASS on765:10loops,20OK/0FAILED,no ERROR,64.948s (767). Original512-file/default-size workload unchanged. Linker reserves PSRAM0x50c00000..0x51000000 for4MiB FS heap, excludes it from12MiB page pool. Earlier703internal-SRAM ENOMEM retained. |
| 1.3.3 RAM patterns | ramtest -w/-h/-b -s 209256 | PASS on700 for all3widths; size from free maxfree, buffer allocated by original test in user heap. Logs705/706/707. |
| 1.3.4 RAM block R/W | mkrd;cmocka_driver_block | PASS840 on837: original stress/single-write/cache-write3/3,2.364s, fresh1MiB RAM disk /dev/ram10; no persistent storage writes. |
| 1.3.5 Flash block R/W | cmocka_driver_block -m /dev/xtsflash | PASS992 on946: original stress/single-write/cache-write3/3,5.337s. Fixed1MiB scratch at0xc00000; double initial blank backups990 identical. Used18256/free17218384 bytes unchanged before/after. |
| 1.3.6 GPIO | cmocka_driver_gpio | BUILD886 PASS, TARGET NOTRUN: J2.13 GPIO47 /dev/gpio0 input and J2.14 GPIO48 /dev/gpio1 output; original API/loop/IRQ runner requires actual IRQ count as well as cmocka PASS. External jumper still required. |
| 1.3.7 I2C/SPI sensor | cmocka_driver_i2c_spi | BUILD887 PASS, TARGET NOTRUN: external BMI160 I2C0 GPIO45/46 on J2.15/16, addr0x68, /dev/accel0. Actual-driver host checks PASS after error propagation/timestamp fixes; module wiring and original100 reads still required. |
| 1.3.10 UART | cmocka_driver_uart -d /dev/ttyS0 -n 0/1/2 | PASS on741:all3modes,official payload and10burst samples,744. Current source splits old combined test into three modes; no device test edits. |
| 1.3.11 UART files | rb -f /tmp/xts805;sb /tmp/xts805/{small,binary}.bin | PASS805 on801:129-byte and65573-byte binary files transferred both directions,byte-for-byte match,normal sb/rb exit. Existing host sbrb.py tail-length and final-ACK bugs reproduced802,fixed803(2host testsPASS); no protocol rewrite. Fresh tmpfs and host fixture directories only. |
| 1.3.12 RTC | cmocka_driver_rtc | PASS841 on837: original API/alarm/periodic3/3,37.343s; absolute/relative timing and alarm readback checked, real SIGEV_THREAD callback logged. Separate FLAT test profile; awake periodic service, not deep-sleep wakeup evidence. |
| 1.3.13 timer | cmocka_driver_timer -d /dev/timer0 | PASS on741,1/1,5.591s,743. Current registered timer driver is timer0; old doc's timer-command typo corrected to actual original program. |
| 1.3.14 24h time | documented date/host comparisons | PASS under clarified project acceptance989. Four recorded stage checks all within2s; elapsed86405.069s, final error[-1.684,-0.663]s. Early first-two sampling times and host capture gap disclosed; no exact-six-hour-spacing or continuous-log claim. Raw evidence988 unchanged. |
| 1.3.15 Watchdog | cmocka_driver_watchdog -r 0/1/2/3 | 868 original mode0 FAILED996 (feeding assertion); preserved log. Candidate999 fixes ROM delay and fatal ISR unintended feed, original modes testing1004. Previous preparation: dedicated M-mode FLAT RTC-WDT driver, CLIC fatal level7, normal capture level1, true RTC-system second-stage reset and board reset-cause mapping. Original mode1 disassembly confirms threshold0xdf. Runner requires panic/stack/hardware reason0x10 for0/1/2 and full API PASS for3; awaits864 completion. |
| 1.3.16 RNG | nist_sts 400000;all15tests;10streams | INCOMPLETE786 on781:162numeric P-values all>0.0001,zero starred rows;26 excursion rows unavailable (only1eligible stream).788 preserves stats:insufficient cycles,not heap exhaustion. Clock gating fix44795df21f4 resolves770's pervasive quality failure. Original algorithms/inputs unchanged; no full PASS inferred from unavailable statistics. |
| 1.3.17 Crypto | eight original cmocka_* crypto apps | Software backend PASS on754/759:3DES1,AES-CBC1/CTR1/XTS1,HMAC3,HASH4,CRC32 4 plusECDSA-P256keygen/sign/verify.757host-parser false negatives fixed and rerun759. 844 on810 also passes all eight original apps; AES-CBC executes28 hardware operations/640bytes/status0, other seven use software. Whole silicon crypto heading not counted as fully hardware verified. Offline998 target1001 passes all eight original functional apps, but SHA/HMAC completion logs show invalid status/byte values; runner rejects those hardware checks. Corrected offline1002 target1010 PASS all eight apps and supported hardware markers/status0. 3DES/MD5/CRC32 remain software. Original883 not flashed because automatic association was not disabled. |
| 3.1.1 12h standby | KASAN + resource monitor, unassociated12h | PASS864 on860:723 frozen records,43329.713s host REALTIME and43320s complete monitor periods; no faults/reset, free522728 bytes unchanged. See checkpoint864-standby12h/result.json and SHA256SUMS-xts864-standby12h. The early automatic label was withheld until actual12h completed. Kernel KASAN_GENERIC/INSTRUMENT_ALL and current SYSTEM_RESMONITOR/showinfo enabled; no NTP and wlan0DOWN. Uses internal FS allocator; process-private heaps excluded from kernel shadow registry. No user-process sanitizer or large-PSRAM-FS-heap KASAN coverage claimed. |

All35 common headings are now visible above. Later category-specific xTS cases
are outside this denominator; neither PASS nor N/A is inferred from custom
tests. BLE/Zigbee last, not removed from scope.

## Latest checkpoint

September15 19:52 +08:00, after the user's internet interruption: read-only
verification found longrun864 PID144782 alive, fresh UART resource samples,
191 resource records, exactly one initial ROM boot and no KASAN/assertion/
machine-trap/panic markers. Elapsed about3h9m; both timed cases remain RUNNING.
No reset or new serial connection was performed for this check. Next scheduled
time comparison remains22:43; do not infer24h accuracy from the initial sample.


Peripheral candidates886/887 built successfully while864 remained running.
GPIO exposes only J2 GPIO47/48, dynamic types, real IRQ counts and one-shot
level delivery; original poll timeout can otherwise falsely pass. Initial885
Kconfig dependency-loop failure retained, fixed in886. BMI160 binds external
I2C0 GPIO45/46 at0x68, legacy character interface matching the original test.
Necessary driver fixes propagate read/register errors and decode the complete
24-bit timestamp. Actual-function host checks pass ASAN/UBSAN; target NOTRUN.
`peripheral-test-guide.md` reserves September17 physical testing and documents
wiring plus guarded commands. `xts-peripheral.py` keeps original tests intact,
requires confirmed wiring, receipt/config validation and an unoccupied UART.
Common count stays25/35. Source/image archive: checkpoint888 and related sums.
Category-only network5.1.41/42 runners are prepared for100 up/down cycles and
100 unassociated scans after the longrun; old10-cycle/20-scan evidence is not
counted as these published cases. See network-case-next-steps.md. No network
source feature was added for this preparation.


883 completes the candidate's ECC-assisted keygen/sign path: optional P256
point hook, verify-then-multiply PIO, hardware constant-time flag, bounded1s
wait and full parameter/key register clearing. Scalar arithmetic remains in
the existing library; projective-blinded ECDH/other curves stay software.
Build883PASS; host884 actual adapter+library integration PASS in native and
no-int128 arithmetic,5point calls each, with signature/negative/ECDH checks.
OpenSSL substitutes point hardware only; no target PASS. Combined candidate
now expects CBC/CTR/XTS, SHA/HMAC, ECDSA verification and ECC point records in
the unchanged8-app batch. Use hardware-ecc runner after864 releases UART.
Archive883/SHA256SUMS-xts884/checkpoint884 retained; count remains25/35.


881 adds optional ECDSA P256 verification in separate ecdsa output. Ordinary
public coordinates/signature/digest are reversed into the locked LL's LE
format; no eFuse key selection/signing call. ECC/ECDSA clocks/lock, bounded
state waits and explicit invalid-signature status. Asymmetric selector now
prefers hardware over earlier-registered software and bounds the op index.
Build881PASS; host882 valid/invalid/length/timeout and actual-selector checks
PASS, ASAN/UBSAN clean. Keygen/sign remain corrected software. Target verification
still NOTRUN. Use hardware-ecdsa runner for the combined Crypto candidate after
864 releases UART. Archive881/SHA256SUMS-xts882/checkpoint882 retained.


879 adds optional HMAC-SHA1/256 via hardware SHA inner/outer digests, ordinary
volatile keys up to64bytes. Dedicated eFuse HMAC peripheral is not used.
Separate hmac output buildPASS; host880 imports6original HMAC vectors,
12single/split comparisonsPASS plus prior12SHA vectors/interleaving,26total
comparisons,63131 host compression calls. ASAN/UBSAN clean. Actual hardware
remains NOTRUN; MD5/3DES/ECDSA still software, long HMAC keys fall back.
Candidate includes878 ECC random-buffer fix; archive879/checkpoint880 and
SHA256SUMS-xts880 retained. Use hardware-hmac runner after864 releases UART.


878 rebuilds the SHA candidate with a necessary four-line ECC correctness
fix: arc4random_buf received NUM_ECC_DIGITS(4) rather than the full32-byte
private/nonce/blinding buffer. Host876 reproduces the incorrect4-byte request;
877 validates full buffers and P-256 operations with/without host int128.
Target original ECDSA remains pending on this corrected image. Use receipt878
for the current SHA output;873 archive retained. SHA and AES mode target
coverage still pending, count25/35 unchanged.


873 optional SHA1/SHA256/SHA512 PIO candidate buildPASS in separate
esp32s31-xts-sha output. Per-session aligned buffers and saved digest state
support the original streaming API; shared AES/SHA lock and bounded wait.
Host874 original9vectorsPASS;875 includes the configured600KiB vectors:
12original +2interleaved comparisonsPASS,63083 host compression operations,
ASAN/UBSAN clean. Host OpenSSL replaces peripheral compression only; target
SHA remains NOTRUN. Hardware runner requires original4/4HASH tests and each
SHA's million-byte/600KiB completion records. MD5/HMAC remain software.
Artifacts873 + SHA256SUMS-xts875 + checkpoint875 archived;864 undisturbed.
competition-demo.md now identifies paired810, entry commands and evidence.


871 adds optional hardware AES-CTR/XTS beside CBC in separate SMP/MMU demo
output.869 built mode logic; source review found crypto_newsession did not
retry parameter-incompatible drivers.871 retries remaining drivers only on
EINVAL/ENOTSUP, preserving hardware-only and resource errors. Host872 checks
the actual selector and actual mode code with original xTS vector tables:
CBC4/CTR6/XTS14,96 encrypt/decrypt single/stream comparisons,1568 host-ECB
calls;3AES192 vectors explicitly unsupported by hardware. Host ASAN/UBSAN
clean. OpenSSL replaces only the PIO primitive: not hardware evidence.
firmware871-aes-modes-build-only.tar.gz, SHA256SUMS-xts872 and checkpoint872
preserve candidate/config/source. Await864 before target verification.


868 watchdog candidate buildPASS and host868 verifies original mode1 uses
MINTTHRESH0xdf, preserving fatal level7.866 missed header declarations;
867 linked but disassembly found exported irq.h overwritten, leaving0xff.
868 fixes the canonical src/esp32s31/include/irq.h. No device flashed or
watchdog test run. Four-mode host runner syntaxPASS; no host reset allowed
between modes. Firmware868 and SHA256SUMS-xts868 archived as build-only.


865 separate xts-flat-flash buildPASS; no board access while864 runs.
/dev/xtsflash maps only0xc00000..0xcfffff. Registration performs no erase,
write or mount. backup-flash-scratch.py preserves two identical reads and
requires all0xff before the original test runner permits writes. Scripts
pass syntax checks. firmware865-flat-flash-build-only.tar.gz and its digest
are archived; hardware result still NOTRUN. No extra xTS PASS counted.


864 longrun is now actually RUNNING after860 build/861 paired flash and hash
verification.862 original memory8/8PASS,4.921s;863 original syscalls83/83PASS,
14.858s.864 started2026-09-15 16:43:12 +08:00, PID144782; host/raw UART and
live JSON use the xts864-longrun prefix. Preserve power, USB and this UART
session through Sep16 16:43. Original showinfo runs every60s; in BUILD_KERNEL
its mallinfo describes its own process heap, so explicit free/ps snapshots
also record kernel/page resources at the comparison checkpoints. Firmware860
is archived separately; SHA256SUMS-xts864 and checkpoint864 preserve hashes,
repository state, tracked delta and untracked port source. The common count
stays25/35 until actual long-duration results arrive.


844 hardware AES-CBC now verified after842 paired flash/hashPASS.843 attempted
while842 still owned UART and was rejected before any command;844 ran after
terminal flash success.28 hardware completions/640bytes, allstatus0. Seven
other crypto applications retain softwarePASS. Functional firmware837 and810
archived and hashed in SHA256SUMS-xts844; checkpoint844-xts.md gives commands.

845 flashes819 KASAN candidate;846 fails before Flash mapping because ASAN
hooks reside in Flash.847 moves runtime into IRAM and stops the retained KASAN
marker before BSS clear.849 now reaches allocator initialization but checker
recurses: CMake applies no-sanitize flags to mm but not the split kmm library.
850 hook-only exception builds; source review confirms allocator metadata
also requires the original Make/mm exceptions, so850 not flashed.851 applies
existing allocator/checker compile flags to kmm as well.853 reaches CPU1 but
fails while reading PSRAM FS-heap shadow metadata.854 changes only the
standby profile to its original SRAM filesystem allocator (FS_HEAPSIZE=0);
SMP/MMU/PSRAM process pages and full kernel instrumentation stay enabled.
856 then exposes syscall argument loss: explicit a0-a6 register variables
are clobbered by inserted checker calls.857 uses normal C ABI arguments;
host856/857 disassembly confirms preservation after the change.859 next
finds process-private heap shadows in the global kernel region list; another
CPU/address environment cannot access them.860 uses the existing nokasan
heap option only for BUILD_KERNEL process heaps. Kernel allocator checking
remains enabled; no claim of user-process sanitizer coverage. These attempts
did not count toward the longrun; all failed boot evidence remains.


Sep15 resumed with user priority: board-only adaptation first; external BMI160,
GPIO jumper and power-cycle fixtures deferred until later, no N/A inferred.
USB3-1 reattached and serial restored. NIST808 actually completed successfully:
100streams,188numeric rows, all threshold checks passed,5372.011s. This is
supplemental evidence; published10-stream786 stays incomplete.

820 flash816PASS but821 cannot boot;824/827/830/833 locate stall at the first
external heap metadata store. PMA candidate825 did not fix it. Datasheet
module table3 and existing production profile require Octal PSRAM; xts-flat
omitted ESPRESSIF_SPIRAM_USE_8LINE_MODE, selecting16-line HAL access.
834 adds8-line config;836 original mmPASS. All temporary boot/heap/trap
instrumentation and ineffective PMA change removed, preserved diagnostic836.
837 clean RTC/heap/block buildPASS;838 flash/hashPASS;839 mmPASS;840 RAM block
3/3PASS;841 RTC3/3PASS. Standard test sources unchanged. F0 dependency lock
stillPASS. Flash guard now supports explicit xts-flat-rtc and checks8-line
mode; both profiles retain the existing persistent partitions.


Sep15 WSL USB restored on BUSID3-1; device group access restored only for
/dev/ttyUSB0. Read-only755b confirms responsive NSH, Kmem225424free,
Page16080896free. Volatile NIST files no longer exist after intervening reset;
752 host log remains the evidence. No source reset or reference changes.

AppFS760 preflash guard blocked oversized3206144-byte image; no flash write.
Root cause: packaging stale applications from reused bin directory. Build762
uses generated current-target manifest, AppFS1343488bytes; stale source apps
preserved. Host tests761PASS, flash763and target764 bootPASS. NuttXe7a950e50d1
commits packaging fix and original NIST template asset integration only.

NIST770now complete and FAIL (see RNG row).774 raw128-byte sample has obvious
counter-like bytes. Source trace: S31 esp_perip_clk_init gates LP RNG after
bootloader_random_enable. Candidate devrandom/devurandom initialization now
reenables RNG once during later driver registration. Build781/flash783PASS;
784b short raw sample no longer shows the counter pattern.785 original syscall
regression83/83PASS.786 full original NIST finished:162numeric rows pass the
documented threshold;26 excursion rows unavailable,zero starred rows.788
captures original insufficient-cycle warnings before resetting. Clock fix
committed44795df21f4. No formal RNG PASS yet, no whitening or statistical test
parameter changes in786.787/791 C++ diagnostics captured790/793;794 fixes
orphan-section placement,795host layout0mismatches,797fullcxxtestPASS.
798helloxxPASS,799memory8/8PASS,800syscalls83/83PASS. Fix7e73da9864a committed
without temporary diagnostics.801/804build/flashPASS;805UART binary round-trip
PASS adds1.3.11. NuttX6c3a1973a61 addsprofile;appsac43243d4 fixeshostYMODEM.
806build/807flashPASS.808 is running an explicitly supplemental100-stream
NIST assessment since approximately12:07+08:00, exclusive UART, no reset
until completion/timeout. Published786 incomplete result remains unchanged.
809/810 hardware AES-CBC candidates buildPASS, not yet flashed or counted;
810 enables non-secret byte-count/status completion records for backend proof.
Separate FLAT original mm/block profile:811missingLIBC_EXECFUNCS dependency;
812hostguard incorrectly required absent BUILD_KERNEL negative line(fixed);
813rwbuffer needs workqueue(fixed by HPWORK);814linkPASS but post-link check
finds original test symbols absent. Cause: source-tree Make-generated builtin
headers override the CMake registry.815 tests compiling registry source beside
its build-local headers, preserving the old source-tree files.
815PASS verifies original mm/block are in linked image;appsfe038f6e4 commits
registry fix.816PASS adds actual HAL-reported free PSRAM interval to the FLAT
user heap using a target-only wrapper of the HAL's no-op registration hook.
Neither816 nor the RTC candidates have been flashed.817RTC buildPASS;
818 fixes32-bit time_t multiplication before converting alarm seconds to
microseconds and also buildsPASS. S31 alarm conversion/rdalarm now use fixed
UTC deadline; periodic support uses existing hardware HR timer service.
819 builds a separate SMP/MMU KASAN/resource-monitor candidate. No extra
headings counted until original target tests run; no reset of active808.

checkpoint744-xts.md records builds727-746, actual first errors, fixes,
commits and target results. Last verified driver firmware741 passes timer743
and UART744; firmware730 passes pipe/popen/MD5/footprint.750is the candidate
NIST stream-cap fix, not yet a verified RNG result. SHA256SUMS-xts730 covers
earlier recovery; newer incremental bundles nuttx/apps/tests-xts744 verified.

## Historical build sequence (superseded where noted above)

- build672: first actual blocker at configuration guard: TESTING_CMOCKA not
  enabled because LIBC_REGEX requires ALLOW_MIT_COMPONENTS. Added dependency
  in test-only profile, retaining local preexisting cmocka source; no fetch.
- build673: four-case batch reaches link; first real error ostest's
  rspinlock_t_test_thread undefined up_cpu_index. Do not just export privileged
  kernel CPU/IRQ operations to user ELF. Full log retained.
- build674: separate three-cmocka profile building. Required ostest profile
  retained as build-blocked; not excluded from acceptance denominator.
- build674 finishedFAIL: memory performance helper ELFs miss
  mmtest_get_rand_size. Added existing kernel/mm/common/test_mm_common.c to
  those two CMake targets, without altering test predicates.
- build675 completedexit0; allthreecmocka binaries verified inAppFS. flash676
  completedexit0 withhashchecks. xts677 actually invokedcmocka_mm_test and
  failedat riscv_createstack.c:161 before any subcase because TLS cap=8192.
- build678 raises CONFIG_TLS_LOG2_MAXSTACK from13to14 in testprofile only,
  preserving standard TESTS_TESTSUITES_STACKSIZE=16384. No assertion disabled.

All builds use `S31_DEMO_PROFILE=<profile> bash build-demo.sh <absolute-new-receipt>`;
full commands in logs/build672-xts-core.log, build673-xts-core.log,
build674-xts-core.log. Configuration and AppFS checks require real enabled
tests and installed binaries. Temporary filesystem /tmp is selected for
test file operations; exact case paths must be inspected before execution.

Historical checkpoint704: flash708exit0, syscall709exit0.
Memory710/scheduler711 original suites being rerun before saving fixes.
The remaining syscall exit assert is traced to task_uninit_info freeing
user-libc cache (getpwuid) via kernel lib_free/kmm_free. Candidate fix uses
group_free for these caches. Target validation709 nowPASS, no teardown assert.
700 increases NAME_MAX32to255, removes temporary scheduler error verbosity,
and adds original fstest/ramtest; assertions and all cases retained.
On690, getprime/scanf/hello PASS.
Firmware686 and690 archived before subsequent builds. New profile adds
documented syscall prerequisites and unmodified getprime, scanftest, hello.
Fixes committed separately: cee97921549 (failed pthread addrenv cleanup),
5a622b5838d (kernel-stack ABI alignment instead of user-TLS alignment).
Test profiles committed868da6f97f7. Bundle nuttx-xts690.bundle, WIP690 archive
and firmware678/683/686 all verified by SHA256SUMS-xts690.
Build696 tests a signal-context fix: restore S31 CLIC privilege/interrupt
return bits from the potentially edited saved xstatus, not a stale extension
slot. Not yet a target PASS. Another syscall blocker: close03 generates a
33-character filename but current NAME_MAX is32; test-only capacity needs
adjusting without shortening the original test's filenames.
Scheduler suites681/682 halted the target. Firmware678 is preserved in
firmware678-xts-core.tar.gz (SHA256 e93c1ebb2fa0ade0242105dbfe2da8aff090f9d1bfbe2ed5445aa884c79f0e80).
Complete previous
recovery image657 is archived in progress654-662.
Failed custom mqueue diagnostic663 remains uncommitted and disabled in normal
and xTS profiles. Keep its sources/logs, do not pursue it now. The interrupted
HTTP670 command produced no log/result and is not counted as PASS.

Test verdicts must inspect per-case output: cmocka_mm_test currently returns0
even if cmocka_run_group_tests reports failures. Exit status alone is not PASS.

## 2026-09-15 品类候选固件补齐（长测期间）

用户确认参赛对象为ESP32-S31 Function-CoreBoard-1开发板本身，按原计划/规格和板载能力核定品类范围。公共25/35不包含品类。

- 网络891：CountryCode getter修复，curl/FTP内核模式构建通过，待上板。
- 文件系统892：原始ROMFS/FATUTF8、压力/稳定性命令构建通过，初始使用RAM测试卷，待上板。
- KVDB895：服务端、UnQLite、完整30项cmocka配置及稳定性应用构建通过，隔离Flash测试卷尚未格式化。
- Sensor896：BMI160-uORB原始两topic订阅固件构建通过，外围实測待9月17日；普通字符接口887仍单独保留。
- 以上仅BUILD PASS，新增品类TARGET PASS为0。详细台账见xts-category-status.md，各编号checkpoint和哈希收据已保存。
- 原始长测864持续运行，20:50串口日志距离检查35秒，无异常复位或错误状态。

后续品类离线补齐：PWM898（单路GPIO48，待API+波形）；SCP901（原始双向传输应用）；
iperf2工具已在904合入网络应用候选（原始四方向300秒，未实测）；
文件系统905补独立Flash/LittleFS及三个原始掉电程序（待真实断电）。
以上均未新增TARGET PASS。各固件已独立归档并有SHA256收据。

September15 21:43: candidate907 filesystem throughput profile BUILD PASS ONLY; original dd statistics and priority255 enabled, isolated output preserves905. See checkpoint907-fs-perf-prepared.md. No target execution; longrun864 continues. Next unused908.

September15 21:53: candidate909 raw Flash dd mapping BUILD PASS ONLY, FTL+BCH /dev/xtsraw covers same1MiB scratch; no boot writes. Schedule after865 and before FS formatting. 908 rejected on BCH flag API mismatch, preserved. See checkpoint909-flash-raw-prepared.md. Next unused910.

September15 21:59: unified competition candidate911 BUILD/PACKAGING PASS ONLY; kernel1173468, AppFS2356224, existing904 network+demo with883 Crypto backends, no Crypto test apps. Isolated out/esp32s31-competition preserves810/883/904. No target run. Next unused912.

2026-09-15 22:47：864 首次 6h 单调计时采样已到；待机/时钟仍 RUNNING。发现 PC 单调计时与日历计时跨度不一致，最终时长和时钟验收需核查，详见 clock864-host-audit.md。原始日志不修改，板卡不重启。

September16 00:25 offline continuation: USB/ADB945 and BLE/bttool939 compiled and frozen, no category target PASS. Flash946 adds internal-stack dispatch for PSRAM callers; raw Flash/KVDB/persistent FS candidates are being refreshed before their target runs. Original864 still owns UART. Common PASS remains25/35.

September16 00:50: refreshed candidates946/947/948/949/950 all built and frozen with PSRAM-stack Flash fix. Audio951 includes original nxplayer/nxrecorder; ADC952 provides actual raw-sample lower half (no calibration), both built/frozen. Source checkpoint953 manifest56files PASS. Read-only observer PID832300/session14576 monitors864 logs and exits on new date sample/fault/process loss; it never opens UART. No active build; next unused954.

September16 independent RNG acceptance review:786 executed all requested
streams;162 numeric uniformity values passed (minimum0.000438), but26 excursion
rows have only1 eligible stream and undefined uniformity. The ten preserved
cycle counts in788 are398,248,10,47,139,109,1177,379,49,64; only1177 meets
the local J>=500 condition. assess.c filters the other streams and its integer
sampleSize/10 becomes0, producing ----. Do not convert this to whole-case PASS
or board-level N/A. NIST SP800-22 Rev1a section5.5 footnote11 also states the
uniformity value is undefined below10 effective sequences; section4.2.2 calls
for at least55 for meaningful analysis. Source: https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-22r1a.pdf .
The original400000-bit/10-stream xTS workload is unchanged;808 remains
explicit supplemental evidence. No extra rerun or driver change is warranted
solely to alter this statistical label. Counts remain25/35.

September16 04:13 timing checkpoint:864 sample2 error is[-0.085811,+0.933120]s,
still within2s. Its automatic standby PASS label occurred after only41401s
REALTIME/41400s complete monitor periods; formal standby stays RUNNING until
43200s actual observation. Original process and showinfo continue unchanged.
No common PASS added. Fresh read-only observer session10148 uses
clock864-observer-0413.jsonl and stops for review on actual12h readiness or the
next date sample.

September16 04:46 actual standby acceptance: original3.1.1 PASS; frozen723
records cover43329.713s host REALTIME and43320s monitor periods with zero
faults. Same864 process remains alive for1.3.14. Image/config hashes, frozen
manifest and standalone evidence archive verified. Common count now26/35;
category formal PASS remains0. Prior timestamped counts are historical.

### 2026-09-16 05:08 — audio968 BUILD ONLY

原始音频默认44.1kHz已完成时钟适配：ES8311按I2S采样率选择MCLK，S31新增11.2896MHz分频，旧48k路径保留。完整构建通过，442116字节；checkpoint968-flat-audio、firmware968-flat-audio-build-only.tar.gz及SHA256SUMS-xts968已保存并验证。963镜像冻结可回退。无上板新增PASS，通用26/35、品类0；864继续。USB965的现有UART刷写入口已补齐并做语法/摘要核验，未刷板。
