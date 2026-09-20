# Competition967 — original WAPI save/reconnect, BUILD ONLY

Supersedes966/911 for the unified demo/network queue. Enables existing cJSON
and WIRELESS_WAPI_INITCONF with /apps/s31-wapi-xts.conf, plus NETINIT_NETLOCAL.
NSH initializes local network parameters but does not automatically associate
from saved settings. Original ifup/reconnect/renew steps remain explicit.
No partition change or format. The three queued original cases are4.1.28,
4.1.30 and4.1.29. All remain TARGET NOT RUN.

Full build/packaging, shell syntax, NuttX whitespace check and paired SHA
verification passed. F0 dependency verification passed before this configuration
change. Kernel1173468 bytes, AppFS2348032 bytes, within existing2MiB/3MiB slots.
WAPI save_config/reconnect and cJSON are retained in the packaged executable.
Frozen pair/config/ELF/map/WAPI/debug binary/profile/helpers/log and manifest
are in checkpoint967-competition-wapi; standalone archive is SHA verified.
Active receipt: build967-competition-wapi.sha256. Receipts966 and911 point to
unchanged frozen pairs.864 remains running on its original verified pair.

Follow host-network-target-sequence.md. Check the new configuration path is
unused before saving; preserve all existing /apps files. Compare saved JSON
privately, store only redacted field checks, and do not re-enter credentials
between save/reboot/reconnect. No new original test assertions or counts were
changed. PTA query and combined BLE coexistence remain unsupported; a WAPI
persistence build does not qualify either feature. Next unused968.
