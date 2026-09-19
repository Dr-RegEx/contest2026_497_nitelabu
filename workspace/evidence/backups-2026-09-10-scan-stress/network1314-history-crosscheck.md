# Existing evidence before next network action

- Existing common30/35 and category28PASS retained; no new run of longrun864/989, scan1112 or storage tests.
- Prior network627 logs prove custom basic WPA2 association/DHCP/ping. They use delayed ESSID plus BSSID and cleanup reboot, not save_config/reconnect original4.1.30. Retain them as supporting evidence; do not fabricate original-case coverage.
- Original4.1.21 now complete through1309 association +1310 continuation; original hostfalsepositive preserved. Do not repeat it without a relevant source/image change.
- Original4.1.28 complete1312; savedconfiguration exists on/apps. Do not overwrite/format filesystem.
- Original4.1.30 incomplete1313; wireless associated but DHCP requestfailed.1314 preserves current interface and unreachablegateway diagnostics.
- Original4.1.26 and4.1.25 pending1311: PTAqueryENOTTY; two identicalRSSI samples.
- Remaining originalnetworkcases require their specific steps; hostbuild-only/toolavailability is notPASS.
