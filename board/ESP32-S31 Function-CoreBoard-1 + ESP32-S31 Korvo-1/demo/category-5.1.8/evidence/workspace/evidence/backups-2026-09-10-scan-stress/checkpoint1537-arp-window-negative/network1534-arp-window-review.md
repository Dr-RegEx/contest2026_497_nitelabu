# Cold ARP resolution candidate

On two fresh boots (1493 baseline and 1528 candidate), the first PC TCP connection failed at arp_wait with -110. DHCP and gateway reachability succeeded. Subsequent ARP readback found the correct PC MAC and then TCP worked, without static neighbor entries. Evidence: checkpoints1527 and1533.

Configured ARP_SEND_MAXTRIES=5 and ARP_SEND_DELAYMSEC=20 give an approximately 100ms retry budget. Candidate1534 changes only this interval to200ms (approximately1s total) relative to1493 configuration. The inconclusive bounded TX poll experiment has been reverted before this build. No application timeout, original xTS acceptance threshold, static MAC mapping, PC firewall, router or VPN setting is changed.

Test plan: fresh boot, authorized reconnect/DHCP, then first TCP connection without preparatory peer ping/static ARP. Use a 10s diagnostic workload and retain both failed and successful attempts. A successful diagnostic is not full xTS performance acceptance; the relevant acceptance remains pending.
