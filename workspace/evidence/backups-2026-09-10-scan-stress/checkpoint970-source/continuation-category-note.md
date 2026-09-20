# Active continuation checkpoint (September15 evening)

Root is continuing without final response per user instruction. Latest user
confirmed the board itself is the competition product. Category audits and
candidate builds891(network),892(FS),895(KVDB),896(BMIuORB) are complete offline,
all TARGET NOT RUN. See numbered checkpoint docs and SHA sums. Original864
longrunPID144782 still active; no serial access since its start. Last actual
host clock20:50 UART age35seconds; process elapsed includes setup time, not
precise board test duration. JSON updates at6-hour samples, not every minute.

Current next task PWM: detailed bounded source implementation instructions
are in pwm-category-task.md. Common esp_ledc is already selected by S31 CMake,
but uses old HAL API incompatible with pinned split esp_hal_ledc. Prefer a
small separate S31 lower-half using verified local LL/group0, no changes to
originalHAL references/common olddriver. No PWM agent has yet been launched
from this note. Originalcase4.2.20 requires cmocka_driver_pwm and waveform.

BMI agent completed source-only changes; root integrated and built896. Existing
apps/testing/testsuites (NOT tests/testsuites) already integrates original
KVDB cmocka. Removed the duplicate new CMake adapter after893 exposed duplicate
targets. Only tests/testcases parent adds existing kvtest subdirectory.895
uses NETDEV_LATEINIT for local-only sockets. Preserve all WIP and archives.

New runners: xts-category-fs.py originalROMFS/FATUTF8 prepared syntax-only;
xts-wifi-lifecycle.py now country in addition to100ifcycle/scan;
xts-peripheral.py now bmi160_uorb, both topic positive counts, three rates
x10rounds,10totalmessages each. No target executions happened.

Update21:25: PWM898 implemented by root (not agent), independent S31 LL lowerhalf
and boardGPIO48 profile, fullbuildPASS. SCP901 builtafter libraryhelpers fix and
SCP_ONLY packagingoption. Originaliperf2locatedexternalrepo, network904 combines
alltools incliperf2 with BUILD_KERNEL HAVE_MLOCKALL adjustment.905 persistentFS
profile includes3powercutapps. AllTARGET NOTRUN. Sourcecheckpoint906 now saves
5repos HEAD/status/diff/untrackedsource and helper snapshots with verified hashes.
Originalobservercell365terminated onlyread-onlyobserver; actual864PID144782alive.
Hostiperf2-linux built successfully using host-only getopt-compat.h (glibcguard
collision, sys/paramMIN) and262144stackdefine; version2.1.8verified. No global
packageinstall. Earlierhostbuilddirs/logsretain failures. No activebuild at this
checkpoint. BLE feasibility taskfileexistsbutNOTassigned; rootread S31bt.c and
new btdm_osal.h only. S31controller has new wr_btdm_osal_* interface, C3oldOSI
tablecannotreuseABI. No BLE sourcechanges. Latestunused firmware/checkpoint907.

September15 21:43: candidate907 filesystem throughput profile BUILD PASS ONLY; original dd statistics and priority255 enabled, isolated output preserves905. See checkpoint907-fs-perf-prepared.md. No target execution; longrun864 continues. Next unused908.

21:49: BLE feasibility agent completed report ble-feasibility.md: ABI compatible but51weak OSAL assertion stubs, real portneeded. ble-osal-implementation-task.md is a prepared taskfile ONLY, not yet assigned. No BLE source modifications. Currentroot inspected I2S LL and schematic only; no audio code edits. KV original execution sequence prepared in kvdb-target-sequence.md; board already mounts /tmp so avoid duplicate mount. Original cmocka returns0 unconditionally; parse30actual passes. 907 dependency lock PASS, archive manifest verified. LongrunPID144782stillhealthy.

September15 21:53: candidate909 raw Flash dd mapping BUILD PASS ONLY, FTL+BCH /dev/xtsraw covers same1MiB scratch; no boot writes. Schedule after865 and before FS formatting. 908 rejected on BCH flag API mismatch, preserved. See checkpoint909-flash-raw-prepared.md. Next unused910.

September15 21:59: unified competition candidate911 BUILD/PACKAGING PASS ONLY; kernel1173468, AppFS2356224, existing904 network+demo with883 Crypto backends, no Crypto test apps. Isolated out/esp32s31-competition preserves810/883/904. No target run. Next unused912.

22:02 checkpoint: all firmware build processes finished. New successful candidates907 FS throughput,909 raw FTL/BCH throughput,911 unified demo are archived with verified manifests. Old908 rejected BCH flags source preserved;910 packaging helper typo corrected911. Longrun alive, latestUART22:00+. BLE OSAL/audio taskfiles remain NOT ASSIGNED, no source implementation has begun. Important schematic verified via PDF coordinates: I2S MCLK52/BCLK53/RX54/WS55/TX56/PA57, matches plan; do NOT use an earlier visual misreading of the thumbnail. Common25/35; no category target passes during864. Next unused912.

22:09: read-only observer exec session59541 started, reads PID144782/logmtime/JSON every55s and exits on new6h sample or anomaly. It never opens UART or sends control/reset signals. Actual longrun remainsPID144782. No active firmware builds.

22:13: no further driver source edits since909, no audio/BLE implementations assigned. Root is awaiting original6h checkpoint with observer59541; not a PASS yet. Source audit confirms time criterion applies at24h, intermediate6h check retained. Original standby requires12h unassociatedKASAN/showinfo; no shortening. All current candidate/source snapshots complete.

Audio candidate915 now BUILD PASS ONLY: profilexts-flat-audio, 421472bytes,
receiptbuild915-flat-audio.sha256/archivefirmware915-audio-build-only.tar.gz,
SHA256SUMS-xts915 verified. Root added board binding/Kconfig/build helpers,
agent /root/audio_i2s completed new arch source. ES8311 existing driver fixed
PCM capability, stale -ERANGE, missing mutex header, channel forwarding,
zero I2S result rejection and error propagation. No UART/reset/flash.
Next unused916. Longrun864 sample1 present; MONOTONIC vs REALTIME discrepancy
requires final acceptance review (clock864-host-audit.md). Raw longrun untouched.
No firmware build currently active. Do not claim formal category PASS.

Latest user progress question answered in commentary with original category IDs: board-only tests are NOT finished; category original PASS count remains0. Audio915 is now new BUILD-only preparation. Existing /root/audio_i2s asked to review the integrated ES8311 changes; no BLE implementation agent assigned yet.

916 replaces915, BUILD PASS ONLY; archive/SHA/receipt916 verified. ES8311 rejects enqueue after termination and STOP returns saved errors, per read-only agent review. Next unused917. No active build. No BLE implementation assigned.


## BLE continuation, Sep 15 23:30

917–924 failed during controller integration; logs preserved. Candidate 925 packaged successfully (811484 bytes) and is frozen in checkpoint925-ble-controller; its receipt now references the byte-identical frozen binary. It is BUILD ONLY and has an identified interrupt-context OSAL issue under repair by ble_osal. Root owns linker/host integration. Original bttool/Zblue profile xts-flat-bttool is building as 926 in a separate output. No serial access or flash; 864 remains RUNNING. Next unused build tag: 927.


## BLE continuation, Sep 15 23:38

928 controller-only image is frozen with receipt redirected to checkpoint928-ble-controller (811016 bytes). Fixed ISR semaphore trywait and identified cache-off closure placement. 929–933 exposed existing BLE-only and local-framework build issues. Fixed ATT BR guards (12 locations plus unsupported connection timeout guard), local adapter debug enum signature, socket-only scan congestion handling, shared SAL async helpers visibility and classic-only connection manager branches. Enabled pthread mutex types. Initial xts-flat-bttool disables optional GATT caching and framework GATT client/server; host connection/security support remains enabled. 934 is currently building in separate output. Next unused 935. No hardware BLE PASS and no UART access. See ble-target-sequence.md for original case mapping and peer requirements.


## BLE continuation, Sep 15 23:50

936 first linked and packaged original bttool plus controller (1209396 bytes). 937/938 (1209732 bytes) add real H4 close, service-loop poll removal before freeing host state, TX/close serialization, partial RX reset, and report BLE OFF only after successful bt_disable. 938 also corrects BLE init-error callback from BREDR OFF to BLE OFF. Artifacts are frozen; SHA256SUMS-xts938 verified. Flash helper now allows xts-flat-bttool with BLE/internal-SRAM guards, but was NOT executed. UART still belongs to 864, live at last read-only check.

939 is currently building to align host/controller advertising capacity at two sets for the three original multi-advertiser cases. 938 receipt was redirected to its frozen byte-identical image before rebuilding. Next unused 940. All BLE candidates remain BUILD ONLY. Category formal PASS count unchanged.

September16 00:18: BLE939 completed and frozen (1209988 bytes), two advertising sets. USB/ADB945 completed and frozen (407284 bytes), SHA256SUMS-xts945 verified. Profiles xts-flat-bttool and xts-flat-usb-adb retain active-output receipts. No active build; next unused946. USB native J4 requires verified VBUS isolation; no USB profile flash whitelist. All BUILD ONLY. Longrun864 PID144782 alive, sample1 only; no serial access. Root is reviewing Flash PSRAM stack dispatch, no changes yet.

September16 00:33: Flash PSRAM-stack dispatcher built946, raw Flash947, KVDB948, persistent FS949 and FS performance950. Each frozen/archived and SHA verified. Older865/909/895/905/907 receipts redirected to byte-identical frozen outputs. Flash/build helpers now require dispatcher when XTS_FLASH and PSRAM heap coexist. Audio951 adds original nxplayer/nxrecorder, built435316 and archived/SHA verified;916 receipt frozen. No active build; next unused952. Root inspecting ADC LL only, no ADC source edits yet. 864 remains alive, no UART access, formal PASS counts unchanged.

September16 00:36: ADC952 built329872bytes, frozen checkpoint952-flat-adc, archive/SHA verified. Adds isolated raw ADC1 CH5/GPIO47 on /dev/adc0, original adc command, bounded polling; no voltage calibration or target samples. Source GPIO analog setup matches locked gpio_config_as_analog. No active build; next953. Root reviewing original864 timing acceptance; original script and evidence untouched.

00:50 continuation: source checkpoint953 frozen,56manifest entries verified, includes nested Bluetooth/Zblue/external changes. Read-only observer PID832300, exec session14576, logclock864-observer-0050.jsonl; wakes every55seconds and exits on new date sample/fault/stale log/process exit. Actual longrunPID144782 unchanged, only samples0/1 so far. No active build; next unused954. Flash runner now requires --receipt for flashblock, config guard and fuser preflight; latest commands in checkpoint946-flash-psram-stack.md. ADC952 and audio951 guides complete, physical guide updated. Root read-only reviewed BLE VHCI deep-copy and original IRQ flags semantics: matches locked HAL, no extra source changes. USB class/pullup sequence retained.

01:00 continuation: ADC954 now replaces952; missing analog I2C root clock and SAR internal bus enable/disable paired with setup/close. BuildPASS 330128bytes; frozen/archive/SHA954 verified.952 receipt redirected to frozen image. No active build; next unused955. Original864 and read-only observer14576/PID832300 still running. Clock source review found periodic hardware SYSTIMER (not relative rearm) and tick-derived system time; no proven cause yet, no clock code/config changes.

01:24 continuation: Native Windows iperf2 2.1.8 host build961 and TCP/UDP 1-second loopback PASS, frozen checkpoint961-host-iperf2-win64. Host-only compatibility patch and config preserved; multicast disabled. Original checkout unchanged. Official Windows ADB37.0.1 version/ZIP validated (host-adb-package.json), no device commands. Dedicated AsyncSSH2.24.0 venv and SCP peer prepared; real legacy SCP bidirectional 1MiB host loopback PASS, checkpoint962-host-network-tools. Private credentials/hostkey stay outside snapshots under host-ssh-private mode0700. Explicit Windows TCP relay prepared, not yet LAN-tested. No LAN server/firewall/system-account changes. Original864 still alive, 518 resource rows at01:20, no fault markers; no UART access. Next unused963.

01:27: fs-short-target-sequence.md reconciles original getopt/positional arguments and actual PASS strings; no test source/count changes. 5.1.7 allocates800000-byte fixture, write_speed defaults1024000 bytes near raw1MiB capacity, fragmentation integer1MiB minimum cannot fit1MiB formatted scratch. Preserve these capacity limits, no silent smaller workload. host-network-target-sequence.md has concrete961/962 paths and commands. Status ledger obsolete raw/dd candidate references updated947/950. Nuttx diff --check PASS,961/962 manifests PASS. No firmware edits/build or UART access.

01:30: Audio963 replaces951 by adding original YMODEM rb/sb with4096 stacks, enabling real speech import and microphone export. Build PASS441600bytes, frozen/archive/SHA963;951 receipt redirected to frozenbyteidentical image. Dependency verification, nuttx diff --check and shellsyntax PASS. No driver/protocol source changes or target execution. Do not run old xts-ymodem.py wholesale onaudio; reuse protocol ononeownedUART withoutresetbetweenrecording/export. No firmware build active; nextunused964.

01:33: checkpoint964-host-scp-relay records native Windows loopback relay to WSL2222 and real OpenSSH keyscan, peer hostkey matched. Temporary relay and SCP peer stopped; noLANbinding/firewallchange. BLE939 existingfrozen binaries now archivedfirmware939-bttool-build-only.tar.gz withSHA939. No newBLEbuild or targetrun. Nextunused965.

01:36: USB965 replaces945 with pinnedusb_dwc_ll16-bit UTMI/timeoutcal5 initialization beforecorereset. Build PASS, frozen/archive/SHA965 verified;945receiptredirectedfrozen. NoUSBtargetcommands or whitelist. Latestaudio963, ADC954, BLE939, Flash946/947, KV948, FS949/950. Noactivebuild; nextunused966.864last533rows at01:35, nofaults.

01:44: Read-only911demo review: paired810/911hashes PASS; packaged s31demo/s31led/curl/scp/iperf2/ftpd_start retainnx_stacksize/nx_heapsize (demo911-app-metadata-audit.json). ftpd_start usesloaderdefaultpriority; otherlistedappsretainpriority. NoELFstackmetadatafixneeded. Corrected preparedhostnetworkboardcommandstoactualiperf2. Demo guide nowrequirestimingextensionifneededbeforeflash.864stillactive; noUARTaccess.

01:46: Prepared64KiB HTTP/FTP/SCP fixture (host-network-64KiB-fixture.json), originalcases nofixedsize.904/911 FS_HEAPSIZE=0 meanshost-only1MiBfixture notassumedtofitRAM. Targetsequenceuses64KiB afterfree/mountcheck; host1MiBproof retained. No newfirmwarebuild, next966unchanged.

02:31: BoundedBLE/WiFicoexsourceauditwrittenble-wifi-coex-source-audit.md. ActualnewBTDMcoexsource hasenabledversion-get return0withoutoutputs (#if0body); KconfigcurrentlyexcludesWiFi. No combinedcandidate/configsourcechange ornewbuild. Preserve939standalone.864continues; next966.

02:43:864 reached601complete resource rows /36000monitor-period seconds and>=36000hostREALTIME seconds. No faultmarkers, free522728/used5652 unchanged.10h observation only, not12h/24hPASS; savedclock864-evidence-audit-10h.json. Onlysamples0/1, observer14576/PID832300 andactualPID144782active. Noactivebuild orUARTaccess. Latestfirmwareaudio963/USB965/ADC954/BLE939; host961/962/964. Nextunused966.

02:52: Preparedextend-longrun-clock.py, syntaxPASSonly, notexecuted. Afteroriginal5samples/finalstate/noerror+UARTfree, optionalsamebootdurationextension usingexistingrawtty withnoDTR/RTS/termiosupdates. Requiresoriginalshowinfo survives; reset/fault/missingmonitorrejectscontinuity, no retry. Sendsps/dateonly; stopsfirstduration-qualifiedcomparison eveniftimefails, writesseparateREVIEW_REQUIREDdata/originalhashes. Next966unchanged; board864 untouched.

02:56: Read-onlyclockcontingencyreview: RTC_HIRES routesclock_systime_timespec throughup_rtc_gettime andexistingfreerunningHRcounterwhenRTC_DRIVERenabled. Candidatefixdirectiononlyifactualfinalclockfails; noRTCconfig/code/buildchanges. Seeclock864-host-audit.md. Longruncontinues.

September16 03:17 continuation:864 still runs unchanged,634 resource rows,
2 date samples,no faults. FTP actual login/passive/get/put instructions added
to host-network-target-sequence.md and syntax checked only. Current category
table references aligned to963/965/949 without changing historical notes or
PASS counts. Independent clock review appended to clock864-host-audit.md;
HIRES remains an unbuilt fallback, not a diagnosed fix. RNG independent audit
confirms786 execution complete but26 uniformity statistics undefined from
only1 eligible stream; maintain INCOMPLETE and808 supplemental. No RNG rerun
or hardware access. No new firmware number consumed; next unused966.

966 competition WAPI candidate BUILD PASS ONLY: cJSON + WAPI_INITCONF,
/apps/s31-wapi-xts.conf; kernel1173468/AppFS2380800. Frozen/archive/manifest
prepared; build966 receipt active,911 receipt redirected to verified frozen
pair. Original save_config/reconnect/reboot cases queued, no target execution.
No source driver changes in this step. Next unused967.

967 supersedes966: NETINIT_NETLOCAL prevents implicit saved-network
association at NSH startup, preserving explicit original reconnect steps and
unassociated scan conditions. BUILD/PACKAGING PASS ONLY; kernel1173468,
AppFS2348032. Frozen checkpoint/archive/manifest;967 receipt active,966 and911
receipts frozen. Next unused968. No target access;864 continues.

04:13 sample2 received, original864 alive. Rawstandby labelPASS is early:
actualwall41401s/monitor41400s, so ledger stays25/35 until actual12h. Clock
error[-0.085811,+0.933120]s within2s. Old observer14576 exited normally on new
sample; root starts a fresh read-only observer baseline3 with12h readiness
event. No serial access/clock change/flash.

04:46 MILESTONE: common3.1.1 actual12h standby PASS, common26/35 now.
Frozen checkpoint864-standby12h and xts864-standby12h-evidence.tar.gz verified;
723 rows,43329.713s host REALTIME/43320s monitor periods, no faults/free522728.
Raw earlyPASS was not accepted until actual12h. ActualPID144782 continues
24h clock run with3samples,last error[-0.085811,+0.933120]s. Observer10148
exited for12h review; start fresh read-only baseline3 observer, ignore already
qualified12h condition. No UART reset/flash/clock change. Latestcandidate967,
nextunused968. CategoryPASS0 unchanged.

September16 05:44 continuation: actual13h observation saved in
clock864-evidence-audit-13h.json:781 rows,46808.021 host REALTIME seconds,
46800 complete monitor-period seconds, no faults, minfree522728/maxused5652.
PID144782 still owns UART,864 time test remains RUNNING with3 date samples.
Common26/35 (standby12h formally frozen), category0. Do not flash until actual
24h time review and any necessary same-boot extension finish.

Audio968 BUILD PASS ONLY adds44100Hz/11.2896MHz MCLK with a default-off ES8311
rate-dependent-MCLK option enabled only in the S31 audio profile. Original48k
and half-duplex transport preserved.442116-byte image, checkpoint968-flat-audio,
firmware968-flat-audio-build-only.tar.gz and SHA256SUMS-xts968 verified.
963 receipt points to its unchanged frozen image; do not use a frozen receipt
with an active-output-only flash helper without restoring the intended output.
Full-duplex nxlooper remains unimplemented: bounded review is recorded in
audio-duplex-feasibility.md. Original4.2.15 is sequential, not full duplex.
Original4.2.16 no-argument app skips both audio directions; a printed PASS does
not establish hardware coverage. Current category ledger records this limit.

Existing local IDF hi_idf_audio.wav copied under host-audio-fixtures; rawPCM
191648 bytes,mono16/48k,unchanged samples. Metadata/hash checked, not listened
or played on board. audio-file-transfer.py prepared for original rb/sb with
os.open on existing raw115200 tty, no requested reset/DTR/RTS changes. Fresh
host evidence, /data tmpfs, absent upload target/existing download file required.
Only syntax/help/source checks run, no UART opened. See transfer-sequence.md.
USB965 added to existing UART flash helper with USB/ADB/internal-memory guards;
syntax and965 digest checked only. J4 VBUS verification/actualADB remain pending.
Crypto883 read-only review found no definite blocker; execution must use
xts-crypto.py --backend hardware-ecc after flashing its paired images.
No active build or host LAN server. Next unused numbered candidate is969.

September16 06:27: audio-file-transfer.py now also offers --sequential
BASENAME to run unchanged original4.2.15 with44100/stereo16/10s defaults,
then export through original sb on the SAME serial handle. It checks fresh
RAM path and2MiB free/contiguous Umem before capture; original cmocka output,
runtime and actual PCM duration are retained. Listening/format acceptance
remains pending. Syntax/help/source checks only; no UART access. See the
updated host-audio-fixtures/transfer-sequence.md and its refreshed digest.

September16 08:34: preparation969 archives board-only original-case runners.
Wi-Fi scan SSID false-failure fixed; BLE original four-step switch runner added;
KVDB original30 runner added with existing raw tty and mounted scratch checks;
Crypto/watchdog reject busy UART before opening. No firmware changes or board
tests/association. Syntax/help/focused host parser checks only. Longrun recovery
PID6841/session13803 continues, remaining18h/24h date checks only. Next unused970.

## 2026-09-16 08:38 +08 — No provisioning constraint

User explicitly prohibited Wi-Fi provisioning. Preserve the current unassociated
run; no association, DHCP or NTP commands. Original 1.3.14 explicitly requires
no provisioning to exclude NTP synchronization, as does standby 3.1.1.
Association-dependent post-longrun steps are deferred; offline preparation and
non-network board cases remain in scope. Recovery collector PID6841 is alive;
resource output continues. No new board command or test result.
