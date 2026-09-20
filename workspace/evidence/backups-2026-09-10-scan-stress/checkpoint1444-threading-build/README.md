# SCP threading candidate 1444

1440 failed linking the kernel: vendor-prefixed Mbed TLS enabled threading from global config, but the ESP32-S31 wireless source lists omitted threading.c. Original failure log preserved.

1444 adds the vendor builtin threading.c to both CMake and Make lists. Its existing implementation uses real pthread mutexes; no no-op callbacks or skipped ssh_init. The translation unit is gated by MBEDTLS_THREADING_C. The competition defconfig enables MBEDTLS_THREADING_C and MBEDTLS_THREADING_PTHREAD.

Build in progress; target test NOT RUN. Current flashed image remains 1430. No xTS PASS added.
