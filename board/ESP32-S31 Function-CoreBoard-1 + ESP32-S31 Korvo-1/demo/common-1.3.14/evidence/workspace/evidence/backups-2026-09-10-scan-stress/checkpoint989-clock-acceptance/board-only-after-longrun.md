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
| 2 | Existing board then946 | Double scratch backup; original common block test | Both1MiB reads identical and blank; boot scratch0xc00000 marker; original complete cmocka results. No format before this. |
| 3 | 947 | Original5.1.13 rawdd write/read |4096x64 each; full262144byte summaries and rates; preserve before filesystem use. |
| 4 | 868 | Watchdog modes0,1,2,3 | Required hardware resets, reset causes, panic stack and final feed/capture results; no host reset to fake a mode. |
| 5 | 883 | Original eight Crypto apps | Use demo-rmt-xts-ecc flash profile and hardware-ecc runner backend, all original results and expected accelerator evidence. SMP/MMU paired image; do not mix AppFS. |
| 6 | 892 | ROMFS4.1.2 and FATUTF84.1.3 | Existing xts-category-fs.py; RAM filesystem only, no persistence claim. |
| 7 | 948 | Original30-case KVDB4.1.11 | Explicit scratch LittleFS mount and /tmp, original30 RUN/OK; preserve database/evidence. No daemon concurrently with FS tests. |
| 8 | 976 | Original short5.1.5/6/8/9/10, boundary5.1.4, rates5.1.7/11/12; capacity-sensitive5.1.2 | Fresh short directories; original workloads; measured rates for community review.5.1.10 alone transfers156.25MiB each direction. |
| 9 | 974 | Original5.1.3 | Disclosed internal SRAM memory config;100 unchanged create/write/remove rounds. |
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
