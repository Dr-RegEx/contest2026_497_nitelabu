# Window64 independent candidate

Profile demo-wifi-rate-window64 inherits demo-rmt-bttool-coex-pie-netdiag-sack-window128-ampdu12, the AP-selection baseline profile. Its only override is CONFIG_UTILS_IPERF2_RECV_BUFSIZE=65536 (baseline 16384). The complete generated configuration is checked byte-for-byte against the AP-selection output after this single replacement. CONFIG_ARP_SEND_DELAYMSEC remains 20; CONFIG_UTILS_IPERF2_TCP_BUFSIZE remains 16384.

This setting is used as mTCPWin in iperf2 Settings.cpp, then as client SO_SNDBUF or server SO_RCVBUF by PerfSocket.cpp and tcp_window_size.c. Despite its name, it affects sender/receiver socket buffer requests in both TCP and UDP modes. It does not change the default TCP application write-buffer length, UDP offered rate, duration, or command workload.

The candidate follows the measured 16k/64k TCP sender comparison. Its independent build is not hardware acceptance. Preserve the original 300-second command workload for target validation, review both endpoint byte totals and actual durations, and retain the complete original and candidate logs separately. A socket limit of64KiB does not guarantee that much payload can be buffered simultaneously with the existing shared IOB pool.

The script build-wifi-window64.sh uses out/esp32s31-wifi-window64, a separate export/build log and a new paired hash receipt. It does not flash the board. The existing baseline and ARP200 profiles are unchanged.
