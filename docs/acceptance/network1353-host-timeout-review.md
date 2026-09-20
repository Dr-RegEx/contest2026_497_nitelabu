# TCP1352 host compatibility finding

PC native Windows iperf2 received2.24MiB over16.67seconds then printed recv failed: Connection timed out. Board original300second command remains under observation; not PASS.

Existing shared iperf2 source Server.cpp configures receive timeout from reporting interval; include/util.h FATALTCPREADERR under WIN32 permits only WSAEWOULDBLOCK. A Winsock receive timeout can therefore terminate this host test instead of returning to interval reporting. This identifies a host-side reason for early session closure; it does not prove that the board has no send or cleanup bug.

Next transport: reuse host-iperf2-linux/src/iperf, which handles EAGAIN/EWOULDBLOCK/EINTR as nonfatal, listening on WSL IPv4, with the existing Windows TCP relay exposing PC port5002. Preserve300second board client workload and both logs. No replacement benchmark or shortened run. Final rate acceptance still requires an explicit applicable criterion; none is numerically given in the source case.
