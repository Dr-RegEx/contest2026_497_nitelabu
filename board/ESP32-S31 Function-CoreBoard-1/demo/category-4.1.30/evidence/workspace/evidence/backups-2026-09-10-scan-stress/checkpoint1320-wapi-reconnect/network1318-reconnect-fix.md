# S31 reconnect ordering candidate1318

Actual1313 saved-profile reconnect associated, then DHCP failed.1314 ping had ENETUNREACH;1316 showed ESSID_OFF, bitrate0,RSSI-128 while old IP remained.
Source970 retention audit proves this adapter path was inherited, not overwritten by audio tests.

Candidate changes only S31 behavior in the shared adapter:
- Wait boundedly for asynchronous disconnect event processing, releasing event mutex while waiting and taking it for the state check.
- Publish updated cached config before reconnect, which otherwise reinstalls the old cache.

Build1318 succeeded. Target1320 original4.1.30 pending; notPASS yet.
Prior1078 image and source1318-before-reconnect remain preserved. No test predicates, original workload, SMP orMMU disabled.
