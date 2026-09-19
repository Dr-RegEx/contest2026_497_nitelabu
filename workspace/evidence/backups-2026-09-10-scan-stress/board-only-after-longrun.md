# Board-only execution queue after longrun864

Final864 review completed at16:43; collector exited and UART is released.
Evidence988 meets actual24h/final<=2s metrics, retains cadence/gap deviations
in raw evidence988. Clarification989 accepts1.3.14 for the project checklist
with actual sample times disclosed; no extra continuous-log/exact-second gate.
The monitoring goal is complete.

Prepared September16. No Wi-Fi provisioning/DHCP/NTP. No operation in this
queue may start before864 actual24h evidence and its sampling/capture deviations
have been reviewed and the sole UART collector has released the port.
Build-only candidates are not acceptance results. Report every completed
original case immediately, including failures, and preserve raw logs/receipts.

| Order | Image / source | Work | Completion evidence / next dependency |
|---|---|---|---|
| 1 | Existing864 | Final1.3.14 date review | Actual>=24h; four scheduled checks with early cadence deviations disclosed, final<=2s interval reviewed; preserve host gap;12h standby archive separate. |
| 2 | Existing board then946 | DONE990/992: backup and original common block test PASS | Both1MiB reads identical and blank; boot scratch0xc00000 marker; original complete cmocka results. No format before this. |
| 3 | 947 | DONE994: Original5.1.13 rawdd PASS |4096x64 each; full262144byte summaries and rates; preserve before filesystem use. |
| 4 | 999 | DONE1016: Watchdog original0→1→2→3 PASS | True hardware resets/causes/stack and original final4/4;996 failure and1004/1008 incomplete transport preserved. |
| 5 | Offline1002 pair | DONE1010: all eight original Crypto apps PASS | Supported accelerator markers/status0 verified; no provisioning. |
| 6 | 892 | DONE1006: ROMFS4.1.2 and FATUTF84.1.3 PASS | Existing xts-category-fs.py; RAM filesystem only, no persistence claim. |
| 7 | 948 | DONE1013 original30/30 KVDB4.1.11 PASS | Explicit scratch LittleFS mount and /tmp, original30 RUN/OK; preserve database/evidence. No daemon concurrently with FS tests. |
| 8 | 976 | Original short5.1.5/6/8/9/10, boundary5.1.4, rates5.1.7/11/12; capacity-sensitive5.1.2 | Fresh short directories; original workloads; measured rates for community review.5.1.10 alone transfers156.25MiB each direction. |
| 9 | 974 | DONE1022 original5.1.3 100rounds PASS | Disclosed internal SRAM memory config;100 unchanged create/write/remove rounds. |
| 10 | 973 | Recovery5.1.18/19 | Automatic reboot+crash for03;10–20s reset for04; no-format remount; preserved nonempty file evidence; test04 patched-source disclosure. |
| 11 | 939 | Original BLE enable/state/disable/state | Callback/state2 then0. No peer, advertisement, scan or coexistence claim. |
| 12 | 968, then979, then985/987 | Original sequential/duplex audio and encoded playback | First qualify968 original independent/sequential recording and export, then979 nxlooper.985 clean build adds original mediatool MP3/AAC/Opus with intact local attachments and native subgraph conversion; boot/internalRAM margin and real playback/listening remain unverified. 987 prepares the intact WAV on a temporary14MiB RAM+3MiB Flash volume; verify separate blank Flash backups and real decoder heap, allow roughly1h upload/readback. Candidate987 final clean build and host driver checks passed; actual target qualification pending. A speaker/headphone or human listening need may remain; no transport-only PASS. |
| 13 | 967 pair | Original unassociated Wi-Fi cases | Country US/CN,100 interface cycles,100 scans; xts-wifi-lifecycle.py requires NETINIT_NETLOCAL and no automatic DHCP. No association, reconnect or network traffic. |
| 14 | Candidate980 pair; verified810 fallback | Offline board demo | s31demo --status and host --offline entry; RGB, codec IDs, BOOT button, resources. Actual980 qualification/observations pending; no provisioning. |

The order prioritizes original common gaps and short board-only cases. A failure
may require a bounded driver fix/build/retest before advancing. No guarantee of
first-flash success. Keep September17 fixture time; do not silently start a
28h filesystem workload that consumes it. The original tests, not an invented
fast replacement suite, remain the acceptance basis.

Timed/storage-tail scheduling: KVDB5.1.68 retains10 original rounds and reset
steps; preserve previous DB evidence before its documented deletion. FS5.1.17
published1h example maps to-t60 in the local minute-based program; corrected976
must finish before it is accepted. FS5.1.15 remains1000 iterations and may be
long. FS5.1.16 permits ENOSPC but its default sleeps alone exceed28h and its
source coverage limits are documented. These are pending and need deliberate
time allocation, not implicit completion. Fill-volume5.1.1 runs last on a
quiescent scratch volume after all other storage evidence has been retained.

USB965 requires confirmed J4 connection/adapter conditions; GPIO/BMI160/PWM,
ADC reference, physical cold starts and peer BLE tests remain for the fixture
session. No provisional build or source audit substitutes for those results.

Existing entry-point documents: checkpoint946-flash-psram-stack.md,
checkpoint909-flash-raw-prepared.md, kvdb-target-sequence.md,
fs-short-target-sequence.md, fs-recovery-target-sequence.md,
ble-target-sequence.md, host-audio-fixtures/transfer-sequence.md and
competition-demo.md, media-target-sequence-985.md, media-target-sequence-987.md and audio-original-resources-984.md. Check active/frozen receipt paths before invoking flash
helpers. Use only the corresponding profile and kernel/AppFS pair.

Preparation boundary after987: all candidates in the board-only queue have
compiled artifacts and explicit target sequences. Remaining work in this queue
needs real Flash/WDT/Crypto/FS/BLE/audio/heap observations and therefore waits
for864 UART release. The complete WAV path now has a17MiB volume candidate,
original-resource capacity proof, bounded driver dispatch checks and guarded
backup/mount/transfer helpers. Hardware success is still unproven. Network
association remains separately prohibited; physical peripherals and listening
observations require the fixture session. Formal xTS counts are unchanged.

September16 target updates: 976 short5.1.5/6/8/9/10 PASS;974 5.1.3 PASS;1023 fixes generic VFS NAME_MAX slash handling and original5.1.4 PASS1028. 5.1.7 random write ENOSPC1029 on1MiB; 1032 independent3MiB candidate ready, no target format yet. 5.1.11/12 original performance rates measured1030; community acceptance pending as source specifies. Country4.1.57 PASS1034, interface5.1.41 100cycles PASS1035; scan100 pending. BLE939 enable stops state1 at1025, diagnostic1031 prepared. Audio968 original sequence cmocka PASS1038, PCM export ongoing; speaker output J9 needs external speaker, no audible playback acceptance yet.

1042/1044: backup d00000..1000000 double-read blank verified;1032 first3MiB format/mount completed1044. Original c00000 volume retained. Audio user now ready:1049 capture/sequence passed original cmocka; export in progress. Switching away invalidates1044 current-boot evidence: after reflash1032 use mount-fs-large-existing.py with fresh output; NEVER rerun first formatter. Original1045/1046 planned runners must reference that fresh evidence before executing. BLE1042 diagnostic candidate prepared (build number overlaps Flash backup1042 but filenames distinct); original1031 Reset path works, later enable remains blocked.

19:08 update: 1049 user listening FAIL (electrical noise), despite cmocka/export success. Boundary diagnostic confirms phase0 RMS 15.36 times other phases; retain intact PCM. Candidate1057 now combines continuous44.1k DMA with ordered zero-length TX FINAL handling; clean build and bounded host check passed, target/listening pending. Use run1058-audio-final-boot.py and build1057-flat-audio-final.sha256 after UART release. BLE1055 fixes missing hardware /dev/urandom, compiled pending original switch test. Current1032 remount evidence1053; original1045 random1000/1000 still running. Do not reset on a host receive timeout; passive resume1045-fs-output.py is prepared for that exact case.

19:49 update: 1059 user microphone listening PASS (clear speech/no electrical noise), original-a3 program1/1 and raw10.031s preserved; J9 external speaker acceptance still pending. 1065 original5.1.2 write1000x1024 PASS21.899s. 1081/1082 original5.1.18 reboot+crash bothPASS. 1083 case5.1.19 fails because unsynced file2 recovered empty; no vacuous checkerPASS. 1085 original BLE enable/state/disable/state PASS on1077 after entropy/buffer/publicMAC/close-lifecycle fixes. 1073 Wi-Fi scan100 failed49th (48completed), PHY temperature callback Load page fault20818000. Category16PASS plus2 performance records pending community acceptance; common30/35. 1087 capture-only4.2.12 executing/exporting on1057, human listening pending.

## 1112/1119 continuation

1112 original unassociated Wi-Fi scan100 PASS on1078, cleanup PASS; category17/common30. Prior1073 fault retained, root cause unproven. 1100 KV ten rounds printedPASS but90 dirty-page errors; rejected, frozen checkpoint1100-kv-stability-errors. Post-error raw volume1110 preserved.1117 releases ONLY archived performance payloads and original required persist.db;1118 reboot/nofmt mount904KiB free;1119 original10 rounds RUNNING, root soleUART, no reset/flash while live. Audio1101 mono16k and FS1102 syncmount independent images built, no board acceptance.

1126 FS5.1.19 PASS on1102 explicit syncmount: both files229376B after15secreset, fulloriginalreader checks passed.1087 userlistening confirmed清晰流畅: category19.1119 KV10 FAIL24commiterrors, volume1122saved; errno-only1130 candidate agentpreparing.1129 monooriginal invocation failedNSH_MAXARGUMENTS7, noPASS; rootneeds1131profile maxargs16, taskdescriptionaudio1131-task.md PREPARED NOT DELEGATED. Board1101audio tmpfs, /data/mic1129.pcm retained, UARTreleased.

1131 rootbuilt/froze monoargs16,1134 original16kmono capture1/1 and323584B export complete; WAVWindows microphone1134-mono-original.wav listeningasked once pending.1130 diagnosticsready,1135flash1136nofmtmount1137onlybackedDBremoved1138requiredreboot/nofmt;1139 originalKV10 RUNNING rootsoleUART. Post1119volume1122backed. Newmount1130identifies real xTS c00000, corrected agent1130doc mistakenlyusingstorage offset180000. No WiFiprovisioning.

20:53 user requested1134retake due missed speech.1139 was deliberately interrupted for user audio priority at4thround, noerrorsobserved butINCOMPLETE, hostpid589589terminated/exit143; fullvolume1140backed.1141flash1131monoargs +1142boot/tmpfs READY, UARTfree, RECORDING NOT STARTED. Rootaskedasync userreply开始, MUSTWAITexplicitreplybeforecapture. Next command audio-file-transfer.py --capture mic1143.pcm --mono16 --receipt ABSbuild1131-audio-mono-args.sha256 --output D/audio1143-mono; usertoldreplythenimmediatelyspeak15s tocoverstartup. Do not reset/switchfirmwarewhilewaiting. Previous1134listeningNOTACCEPTED because speechmissed;1087userconfirmedclearremainsPASS/category19. Afterretake/export/listening requestresume otherboardwork.

1144 KV3MiB candidate built/frozen checkpoint1144-kv-large, receiptbuild1144-kv-large.sha256. Changes only existing XTS_FLASH_LARGE selector from1130diagnostic;originaltest10/backend/commit unchanged. Not flashed or tested. Board remains1131audioargsready1142, awaitactualuser开始before1143record.1140KVbackupread-only mounts106/256blocks,DB307200B; nopeakcapacityproof.

1143 originalmono16k retakecompleted and userconfirmed fullclear speech, normalrate,noelectricalnoise: category20/common30. Fullcapture+listen checkpoint1143-audio-mono.1145flash1078,1146boot,1147offline demo commandsCOMPLETE:radio down/IP0,RGBframes4,codec83/11@100k/400k,2coresandheap/fs. RGB userconfirmationasked pending;buttonsnotyetexecuted. Currentboard1078offline, UARTfree,noassociation.1144KV3MiB candidate frozen,notflashed.

1149 real BOOT press1/release0 PASS,task11killed/psclean.1148hostPIDregexCRLFfailed and thenresumed sameprocess, logsretained.1150full3MiBvolume backup SHA3b6c77393e0aed0ba5e9830922da00a3f02ec25ba95c98d7ad50872f5bed09c2, noformat.1151flash1144,1152nofmtmount,1153DBalreadyabsent,1154requiredreset/remount freespace691/768blocks.1155 originalKV10 RUNNING rootsoleUART, session51451; script run1144-kv-large-stability.py, loglogs/xts1155-kv-large-stability.log. Do not flash/reset/competeUART while live. RGBquestionpending; category20/common30; goalactive.

1156 host preparation: run1156-fs-one-hour.py is ready for original5.1.17 one-hour example on frozen1102 explicit sync mount. Local minute units require-t60; prior worker-join source repair remains disclosed. Syntax/help checked, NOT RUN. First finish/archive1155 and preserve its post-test volume; then fresh1102 flash/boot/mount, no formatting or stale1125 mount evidence. No new PASS.

1158 offline source work while1155 owns UART: atomic DMA error publication/clear compiled as standalone RV32 object; agent added BUILD_KERNEL ES8311 privileged worker/completion lifecycle with flat preprocessing equivalence and kernel object checks. Not a new firmware or board PASS; APB address environment/lifetime still blocks kernel audio. Kconfig guards retained. Root reviewed worker start/failure/STOP/release paths.

用户再次明确优先级：先完成测试集，不做细枝末节。暂停SMP/MMU音频合并与其后续审计；1158仅保留未上板前置改动及证据，不进入当前验收镜像。当前1155继续原始KVDB十轮，完成归档和卷备份后直接运行1102/1156文件系统一小时用例。仅修复阻碍原始测试通过的问题。

1155原始KVDB十轮PASS，无IO/VFS提交错误，最终DB405504B，可用591块。checkpoint1155-kv-large-stability归档；完整3MiB卷1159 SHA dd17bfe734afb054ce52322128b600694b2b7d5cb6b971d9ff1509e4e0b10240。1160烧录1102、1161启动、1162无格式化sync挂载完成。1163文件系统5.1.17原始三线程-t60运行中，PID619827/session11563，日志logs/xts1163-fs-one-hour.log；独占UART，无复位/刷机。通用30/35、品类21PASS。

5.1.16验收口径核对：原文只要求“运行一段时间后无异常”，没有指定28小时。28h是当前程序默认完整循环的耗时估计，不能额外升格为官方必测时长。后续应记录实际运行区间、观察到的轮次和停止方式；未跑满默认循环就不宣称完整1000外层循环完成。-n/-s可以按原文明示的资源条件选择，不能隐藏使用原文未列出的-c/-N缩减次数。此核对没有执行碎片测试，也没有新增PASS。

**1163新增PASS：5.1.17文件系统三线程一小时用例完成，实际3921.870秒，三线程正常退出，原始PASS及返回0，无错误。1102镜像、3MiB LittleFS显式sync挂载，保留已披露worker-join修复；耗时延长原因未证实，不作时钟精度结论。证据checkpoint1163-fs-one-hour。品类22项PASS，通用30/35（85.7%）；UART已释放。**

**1164运行中：原始5.1.16碎片测试，按资源设置-n100/-s1，未用-c/-N缩减程序循环；计划观察首个完整外层循环（100次大文件创建/删除）后明确停止，按原文“运行一段时间后无异常”记录实际观察范围，不宣称默认1000轮完成。当前1102镜像、独占UART/session76979，不配网，不复位。品类22项PASS，通用30/35。**

**用户最新指令（9月17日）：完成当前1164碎片测试后暂停。仅完成已启动观察范围、归档结果并汇报，不启动后续测试或适配，等待用户恢复。**

**9月17日已按用户要求暂停：1164碎片测试观察4579.232秒，100小文件/首轮100次大文件创建删除成功，原程序首轮OK；终止任务未退出，1165保留收尾异常，1166进入ROM并硬复位停止残留任务，无刷写、无新测试。暂不增加PASS，品类22项、通用30/35。证据checkpoint1164-fragment-observation。等待用户恢复。**

**9月17日用户已明确恢复。1167启动/1168无格式化挂载通过；1169复核5.1.16按原文观察标准通过：4579.232秒、首轮100次大文件操作及小文件阶段无错误，首轮OK；不宣称1000外层循环完成。kill收尾限制定位为1102未启用CONFIG_SIG_DEFAULT，保留失败脚本和复位记录，不追加非原文终止门槛。品类23项PASS，通用30/35。**

**1173新增PASS：原始5.1.1分区写满异常测试完成，主机126.708秒、程序报告103.52秒，原始TEST PASSED/返回0，测试文件自动清理，任务退出；保留原程序不报告终止写入errno的覆盖限制。证据checkpoint1173-fs-fill。品类24项PASS，通用30/35，串口已释放。**

1176 physical RESET capture READY, session88025,10min passive window, user action requested. No host reset/flash, no other UART owner while waiting. User confirmation and clean ROM-to-NSH boot required; not PASS yet.

1183 host-verdict correction prepared: fstest_directory source labels all non-file entries Error, including exact Type[4] . and .. entries, then returns OK. verify1183-fstest-log.py exempts only those exact lines; rejects other diagnostics and incomplete loop/summary coverage. Running1181 process is unchanged and WILL reject its accumulated dot-entry errors at final verdict before echo/cleanup. After that host exit, do NOT restart the board test: review the same complete log with1183, then obtain original echo $? and documented cleanup on the unchanged board. No new PASS now. Current pid669451/session73058 UART owner. Windows keep-awake1182 session17683 active, 12h maximum.
