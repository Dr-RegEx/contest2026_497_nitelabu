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
