# Original iperf2 network candidate 904

Full offline build/packaging PASS; TARGET NOT RUN. Kernel1027968 bytes,
AppFS2356224 bytes, paired build904-netapps-iperf.sha256. This adds the existing
external/iperf2 Iperf2.1.8 to901's curl/FTP/SCP/CountryCode candidate. The
original netutils/iperf remains a different tool; no renaming or downloaded
repository was used. Original workload/statistics code remains unchanged.

External config.h now advertises HAVE_MLOCKALL only outside BUILD_KERNEL.
NuttX fs/mmap/fs_mmisc.c has the implementation, but no syscall export exists
for separate user ELF applications. The optional realtime mlockall path caused
902's link failure.903 built with it disabled;904 preserves the existing FLAT
capability and is the final intended config. Standard300s/-b40M workloads do
not request realtime locking. Runtime allocation/performance remains untested.

Prepare all four original2.4GHz directions using the published ports:
5.1.22 TCP RX5001;5.1.23 TCP TX5002;5.1.24 UDP RX5003;
5.1.25 UDP TX5004. Every stream lasts300seconds. UDP sender -b40M is the offered
load, not a promised achieved throughput. Record router model/settings,
environment, both program versions, duration, actual throughput/loss and logs.
Published text says meets standard without a numeric threshold; retain actual
measurements and do not invent an acceptance threshold. The mis-titled4.1.1
filesystem heading also contains an iperf TCP workload; audit it by its body.

Pair904 has no new board/network evidence. Use the existing private credential
flow; keep passwords out of command history/reports. A PC on the same LAN is
required for original client/server traffic, but not a new external repository.
