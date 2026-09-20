# DNS resolver prerequisite repair

1330: original curl www.baidu.com returned error6, getaddrinfo thread failed to start. Same1328 firmware passed original IP-based HTTP download1331.

Local source: external/curl/curl/lib/asyn-thread.c initializes AF_UNIX SOCK_STREAM socketpair before creating its resolver thread. curl_config.h defines HAVE_SOCKETPAIR; socketpair.h maps directly to the OS. The1328 configuration disabled CONFIG_NET_LOCAL.

Candidate1332 enables NET_LOCAL and NET_LOCAL_STREAM in the competition profile, leaving the original curl resolver and test unchanged. NET_LOCAL_DGRAM remains disabled. The prior TLS fix and SMP/MMU remain enabled. Runtime success is not assumed until original4.1.114 returns HTML.
