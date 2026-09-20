# Native iperf2 peer prepared

Existing local source external/iperf2/iperf2 was built for Linux without
re-downloading or modifying that source tree. Executable:

`backups/2026-09-10-scan-stress/host-iperf2-linux/src/iperf`

Version output: iperf version2.1.8 (12August2022) pthreads. --help validates
bandwidth, UDP, time and server/client CLI options. No board or LAN throughput
test has run. This host helper is not an xTS target PASS.

The Vela fork has an unconditional CONFIG_UTILS_IPERF2_STACKSIZE use and an
old bundled gnu_getopt.h sharing glibc's _GETOPT_H guard. Host CPPFLAGS set
thread stack262144 and preinclude host-only getopt-compat.h, which includes
Linux getopt/sys/param before the original prefixed GNU declarations. This
also provides MIN used by the fork. Firmware source/protocol code is unchanged.
All attempts and the successful native build are logged as host-iperf2-*.
The final build directory is host-iperf2-linux; the earlier host-iperf2 directory
contains failed build evidence. No system package or global install changed.

For original device-RX tests, start the device server then run this host client:
TCP: iperf -c <board-ip> -i1 -p5001 -t300
UDP: iperf -c <board-ip> -i1 -p5003 -t300 -u -b40M
For device-TX, start this host server first:
TCP: iperf -s -p5002 -i1
UDP: iperf -s -p5004 -i1 -u
Then start the matching original board client for300s. Use actual LAN-reachable
host/board addresses; WSL NAT/interface routing still needs checking at run time.
Record environment/router settings, both version outputs, raw logs, interval
and aggregate results/loss. Stop only the server PID launched for this test.
