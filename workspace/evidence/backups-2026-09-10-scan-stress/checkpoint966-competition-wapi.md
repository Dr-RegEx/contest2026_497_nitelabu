# Competition966 — original WAPI persistence commands, BUILD ONLY

The911 competition profile lacked WIRELESS_WAPI_INITCONF, so original cases
4.1.28(save_config),4.1.30(reconnect),4.1.29(reboot/reconnect) could not run.
Enabled the existing local cJSON dependency and WAPI initialization support.
The configured file is /apps/s31-wapi-xts.conf on the existing writable volume;
no partition, automatic format or network credential change. Follow-up review
found that default NSH netinit can automatically load saved settings on reboot;
967 adds NETINIT_NETLOCAL to keep association explicit for the original cases.
The build helper checks these options and the packaged WAPI application.

Full build/packaging, F0 dependency verification, shell syntax and NuttX
whitespace checks passed. Kernel1173468 bytes; AppFS2380800 bytes. The packaged
WAPI binary contains save_config, reconnect and the exact configured path.
Frozen pair/config/ELF/map/WAPI/debug binary/helpers/log are in
checkpoint966-competition-wapi, with a checked manifest and standalone archive.
The build966-competition-wapi.sha256 receipt now refers to the frozen pair.
911 remains byte-identical in checkpoint911-frozen-pair; its receipt points
there.864 images still verify against their original receipt and run unchanged.

No target execution. Use host-network-target-sequence.md after864 and common
validation. Preserve existing /apps contents; create only the identified new
configuration file after checking it is unused. Verify saved fields privately,
keep secrets out of raw UART logs/archives, and retain redacted comparison
results. Reboot case must reuse the saved credentials without re-entering them.
No whole Wi-Fi/coexistence claim: PTA getter is still unsupported; country
getter exists but remains unverified on target. This candidate supersedes911
for the unified demo/network queue. The older904 image remains unchanged.
