# Original throughput buffer prerequisite

1352 failed: board TCP send could not append to IOB chain after15seconds, no shell return through360seconds of idle observation; PC timed out after16.67seconds. Not a300second PASS.

1332 had NET_SEND_BUFSIZE=0, which compiles out the TCP queued-byte backpressure branch; original iperf requests16KiB send buffer but reports unsupported socket window(-1). IOB pool is96x400bytes with24 reserved. Candidate1354 sets NET_SEND_BUFSIZE=16384, enabling existing bounded-send logic and socket option support without reducing original application buffer or duration. No generic TCP implementation change or new artificial rate limit.

This is a candidate, not established root-cause fix. Run the original300second workload against the existing Linux peer to avoid the identified Windows receiver timeout behavior; preserve both endpoints and queue-error observations.
