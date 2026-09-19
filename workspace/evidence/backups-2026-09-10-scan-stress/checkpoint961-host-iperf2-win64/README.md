# Windows iperf2 host candidate 961

Built from the existing local iperf2 2.1.8 checkout; original sources unchanged.
Host-only patch uses modern Winsock declarations and converts 32-bit timeval
seconds to UCRT time_t by value before localtime. host-compat.h provides MIN
and bool compatibility. Configure with --host=x86_64-w64-mingw32
--disable-multicast, CFLAGS="-O2 -std=gnu11", and CPPFLAGS containing
-DCONFIG_UTILS_IPERF2_STACKSIZE=262144 -include <absolute host-compat.h>.
Build with the existing MSYS2 UCRT64 compiler and make.

TCP and UDP 1-second native Windows loopback checks passed. This validates
host executable setup only, not target throughput or any xTS result.
Multicast is disabled; planned original cases are unicast, 300 seconds each.
No board UART, firmware, Windows firewall, or system service was changed.
