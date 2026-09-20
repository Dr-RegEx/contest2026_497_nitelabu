# Network priority after user awake

User authorizes network adaptation after mandatory unassociated cases.
Verified local original test source has two explicit unassociated requirements:
- 3.1.1: checkpoint864-standby12h/result.json PASS.
- 5.1.42: checkpoint1112-wifi-scan/result.json PASS100, associated=false.
The24h clock case is separately accepted989; no repeat longrun needed.

First19 pending cases with existing PC and ordinary WPA2 2.4GHz AP:
- 4.1.21/25/26/28/29/30/31 (7).
- 5.1.26/27/29 (3).
- 5.1.22/23/24/25 (4), original300s each.
- 4.1.113/114/115/116/117 (5).
This is a runnable-priority count, NOT all remaining project cases.
Special router modes/encryption/channel sweeps, multiple routers, phone hotspots,
shielded RF, BLE peers and Ethernet remain separately unverified.

Audio1295 failed completion after180.033s, savedcheckpoint1296.
Diagnostic1297 built successfully but NOT flashed/run. Preserve it for later.
Switch to frozen1078 paired kernel/AppFS, retaining persistent testFlash.
Current AP choice requested; do not expose keys in logs/archives.
