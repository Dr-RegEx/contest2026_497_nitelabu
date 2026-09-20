# Offline host entry supplement981, no new firmware

Use with candidate980. The host tool explicitly runs
prlimit -s8192 s31demo --status (with spaces between option/value) so stripped
AppFS symbols cannot silently reduce the application's configured8KiB stack.
No target command was executed. Rechecked offline good/bad-ID paths with fake
UART and actual C status/strace. Existing Apps test_host.py passed on host
loopback: HTML/JSON/repeat/method/path/browser-headers/header-limit. These are
host implementation regressions only, not board or xTS acceptance.

This supplement preserves original980 archive; images and ABI pair did not
change. Current source and README are captured here. No provisioning/reset.
