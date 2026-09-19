# Competition demo: reproducible entry points

## Current execution priority (1354/1360)

The user has authorized the specified WPA2 network. Unassociated tests are complete. Follow `priority1352-network-and-demo.md`: TCP/UDP original workloads, connection stability, then demonstration/delivery integration; defer individual SCP and audio defects when they do not block the demonstration.

Current candidate is the SMP/MMU ABI pair in `build1354-network-sendbuf.sha256`. It includes the original demo app plus the validated reconnect, reboot, curl prerequisite fixes and a new16KiB send-backpressure configuration. TCP1360 is still running, not accepted. Previously frozen images and old PASS remain authoritative only for their recorded cases; current common30/35 and category35PASS.

After the current UART owner finishes, the intended network demonstration sequence is: confirm the saved-profile connection and DHCP, run `s31demo --status`, run the existing `s31demo wlan0` HTTP service, and open its published page from the PC. Inspect `s31demo` source and its existing helper before execution; this paragraph does not assert a new demo run or require resetting an already connected target. Record the final pair and visible result. The board status/RGB/BOOT evidence1147/1149 remains reusable; only checks affected by the selected final image need rerun.

Audio/BLE remain separate tested profiles. An integrated all-feature image is not yet proven. SCP KEX fault1351, DHCP intermittent stall1343, initial PC ARP failure1357, and audio Flash playback remain documented limitations.

The sections below are retained historical instructions and provenance. Their old no-provisioning restrictions and live-transfer status are superseded by this current section; do not restore an older image while an active test owns UART.


September17 update through1262: longrun864 is complete under acceptance989.
Common30/35; category26 accepted. Current authoritative per-case evidence is
`xts-current-status.md` and `xts-category-status.md`. Multiple frozen images
provide this coverage; no single final demo image has passed all cases.

Latest board evidence: BLE complete enable/state/disable/state1085; Wi-Fi
unassociated original scan100 on1078 passed1112, while prior1073 page fault
remains preserved and its root cause unproven; filesystem reset recovery1126
passed on1102 explicit syncmount. Audio1059, original capture1087, and16k mono retake1143 have user listening
confirmation.1134 was not accepted because speech began too late.
KVDB basic30 cases1013 and original10-round stability1155 passed on the
3MiB configuration; earlier1MiB failures remain archived. Original fstest1000
passed1184. AAC1249, Opus1251 and MP31253 completed original digital playback
and cleanup; audible acceptance remains pending. Intact WAV1262 is currently
being transferred, not accepted. Original failed logs remain preserved.

Run commands from `/home/regex/work/esp32s31-openvela`. Root coordinates UART for the current test; check actual ownership before any
reset, flash or serial operation, and do not interrupt an active test.
1078 offline command sequence was executed in1147: status, RGB driver, codec IDs,
free/ps/df all completed; BOOT press/release passed1149 with task cleanup,
RGB optical review pending. This is the current
board evidence (checkpoint1147-demo-offline). Historical980 was not separately
flashed. Audio/BLE remain separate validated profiles. Live networking
is deferred by the user's no-provisioning instruction. USB/external speaker,
GPIO loopback and external sensor acceptance still require physical setup.
The older candidate sections below describe provenance, not current counts.

## Current offline demonstration image

Use the1078 ABI-paired kernel/AppFS for the current offline presentation.
Both files were rechecked against `build1078-wifi-mmu.sha256` on September17.
Actual evidence: scan1001112, offline status/RGB-driver/codec1147 and physical
BOOT1149. RGB optical review and a final presentation recording remain pending.
The old967 page fault has not acquired a proven root cause from1078 passing.
Audio/BLE are separate validated profiles, not features of this image.

After the active WAV workload ends, preserve its evidence and restore the
saved3MiB xTS scratch using the verified1255 backup before selecting another
profile. Never run this reset/flash command during the transfer or playback:

```sh
bash backups/2026-09-10-scan-stress/flash1078-wifi-mmu.sh "$PWD/backups/2026-09-10-scan-stress/build1078-wifi-mmu.sha256"
```

Use the existing offline demonstration flow below. Keep Wi-Fi unassociated;
no HTTP/network-start command is authorized by these offline instructions.
1078 is an offline demonstration baseline, not all-xTS or integrated-audio PASS.

## Historical810 image and provenance

The retained historical demo output is build810, paired kernel and AppFS:
`openvela-dev/out/esp32s31-cmake-demo`. Its receipt was rechecked on Sep15;
both images match `build810-xts-aes-cbc-proof.sha256`. It includes two-core
SMP/MMU, Wi-Fi b/g/n, HTTP status, board RGB/button/I2C and original xTS apps.
Flash842 and crypto844 verified this exact pair on the board. HTTP/RGB/I2C
concurrent evidence is from earlier406, not a new network run on810.

After the longrun and any required same-boot timing extension finish and their
results are recorded:

```sh
bash backups/2026-09-10-scan-stress/flash-demo-pair.sh "$PWD/backups/2026-09-10-scan-stress/build810-xts-aes-cbc-proof.sha256"
```

The guard rejects a busy UART or changed image hashes. It writes only the
paired kernel at0x2000 and seed AppFS at0x200000; writable `/apps` starts at
0x500000. The independent archive is
`firmware810-hardware-cbc-verified.tar.gz`, checked by `SHA256SUMS-xts844`.
Do not substitute build946/868 FLAT xTS images for this network demo.
Combined candidate883 adds AES modes, SHA/HMAC, ECC-assisted keygen/sign and
hardware ECDSA verification; it still awaits board verification.

## Current execution restriction: no provisioning

User explicitly prohibited Wi-Fi provisioning on September16. Do not run the
network-start tool below, WAPI association, DHCP or NTP while this restriction
is active. The completed864 longrun no longer holds UART; any current active test does.
After the active test releases UART, board-only demonstration may use the existing
RGB, codec-ID, BOOT-button and resource commands below, without starting HTTP
or configuring a network. HTTP/live network presentation remains deferred,
not considered complete by offline command preparation.

## Network presentation (deferred)

Once Wi-Fi provisioning is authorized again, start networking using the existing interactive tool; enter the authorized
WPA2 credentials privately at its prompts, never in a shell command or log:

```sh
s31-reference/.venv-nuttx/bin/python openvela-dev/nuttx/tools/espressif/esp32s31_demo.py --port /dev/ttyUSB0 --log backups/2026-09-10-scan-stress/logs/competition-demo-start.log
```

Use a fresh log filename on each attempt. The tool resets once, scans,
associates, obtains DHCP, checks the gateway and starts the HTTP app. Open
the printed `http://<board-ip>:8080/` from a client on the same LAN. Show its
uptime/address and `/status.json` request counter. The page is read-only.
Use the newly printed address; historical log addresses are not current.

For the board-only presentation after longrun release (or after the network
tool releases UART when networking is authorized), use the NSH console:

```text
s31led 1
i2c get -b0 -a18 -rfd -w8 -f100000
i2c get -b0 -a18 -rfe -w8 -f400000
buttons &
free
ps
df -h
```

RGB should show low-brightness red/green/blue/off; codec IDs should be83/11.
Press and release BOOT to show button notifications; release it before any
reset. Stop the button task with `kill <pid>` using the actual reported PID.
Codec ID reads demonstrate the control bus, not audio playback. When networking is authorized, the HTTP demo can remain running while these
commands execute. The offline presentation does not imply live HTTP verification. No external sensor or
GPIO jumper is needed for this presentation.

## Present acceptance honestly

Use `xts-current-status.md` and its original logs for the35 common headings:
currently30 accepted, with each remaining case explicitly pending/running.
Do not represent all tests as passing in one configuration: heap/block/RTC
have a separate FLAT image; the primary demo uses SMP/MMU. Long-duration
KASAN results belong to860. AES-CBC hardware is verified on810; combined Crypto883
is build/host checked only. BLE/Zigbee, audio playback and external-fixture
acceptance remain unfinished. See checkpoint864/865/868/872 documents for
the queued work and exact artifact provenance.

## Unified candidate967 (not yet board verified)

Independent output `openvela-dev/out/esp32s31-competition` combines the existing
demo, original curl/FTP/SCP/iperf2 and the883 Crypto backends within existing
partitions, plus original WAPI save_config/reconnect support. Receipt:
`build967-competition-wapi.sha256`; checkpoint967-competition-wapi and its
archive retain the pair. The old911 pair remains frozen separately.
After864 and prerequisite validation,
select `S31_DEMO_PROFILE=demo-rmt-netapps-competition` for flash-demo-pair.sh.
Use the same presentation flow above. Keep810 as the known fallback until967
passes actual boot/network and concurrent board-feature checks. Missing audio,
BLE, USB ADB and OTA remain visible; unified packaging does not establish them.

September16 preparation: audio968 and USB965 are separately built fixture
candidates, BLE939/ADC954 likewise. They are not integrated into967 and are
not evidence that967 has those drivers. The network command name in the
actual AppFS is `iperf2` (not `iperf`); host commands are documented in
host-network-target-sequence.md.

Plan cross-check: the original15-day plan marks YT8531 Ethernet asP1 and
M4 asks for at least two of Ethernet/Audio/BLE/USB end-to-end demonstrations.
No S31 EMAC/YT8531 board integration was found in the current openvela S31
arch/board sources. Ethernet remains unimplemented/unverified, not silently
N/A. The current Wi-Fi/core demo alone does not satisfy that M4 milestone.
Audio/BLE/USB candidates must first demonstrate real hardware operation.
Per the user's current xTS-first instruction, no extra custom Ethernet suite
is added simply because the old plan proposes one.

September16 09:36 offline check: both active810 and967 kernel/AppFS receipt
hashes match their files. No flash/demo command was executed. New filesystem
candidates973/974/976 are separate test images, not replacements for the final
SMP/MMU presentation. Final live video and actual967 hardware qualification
remain outstanding; this document is not a claim of a completed final demo.

## Offline presentation candidate980 (BUILD ONLY)

An independent demo-rmt-netapps-competition-offline profile now includes
`s31demo --status` and the host `esp32s31_demo.py --offline --receipt ...` entry.
Use checkpoint980-competition-offline.md for exact flash/presentation commands
after longrun release.967/810 remain preserved. This displays actual status,
RGB and codec IDs without provisioning; BOOT/optical/audio reviews require real
observation. It does not merge the incompatible FLAT audio/BLE/USB profiles or
satisfy their xTS cases. No new board PASS.
