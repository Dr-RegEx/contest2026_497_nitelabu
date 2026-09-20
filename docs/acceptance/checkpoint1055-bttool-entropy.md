# BLE 1055: provide the hardware entropy device required by PSA

BUILD PASS, TARGET NOT RUN. The missing entropy endpoint is confirmed in source/configuration; its correction still needs the original board test. BLE is not yet a PASS.

1051 on candidate 1042 completed HCI opcodes 0x0c03, 0x1003, 0x1001, and 0x1002 with status zero, then timed out awaiting adapter state 2. Immediately after those commands, `common_init()` calls `prng_init()` because `CONFIG_BT_HOST_CRYPTO_PRNG=y`.

Source chain:

- Zblue `subsys/bluetooth/host/crypto_psa.c`: `prng_init()` invokes `psa_crypto_init()` and returns `-EIO` on failure.
- Mbed TLS `library/psa_crypto.c`: initialization seeds the PSA random generator and returns any seeding failure.
- Mbed TLS `library/entropy_poll.c`: the NuttX platform entropy path invokes `getrandom(output, len, 0)`. Errors other than ENOSYS become `MBEDTLS_ERR_ENTROPY_SOURCE_FAILED`.
- NuttX `libs/libc/misc/lib_getrandom.c`: flags zero opens `/dev/urandom`; it does not use `/dev/random` as fallback.
- Candidate 1042 enabled `/dev/random` but disabled `/dev/urandom`, and did not configure alternate hardware entropy. Thus this source cannot seed successfully. This is an immediate missing-device error path, not evidence of an entropy-pool blocking wait. Framework logging can hide the initialization error.

Minimal correction in `xts-flat-bttool/defconfig`: enable `CONFIG_DEV_URANDOM=y` and explicitly select `CONFIG_DEV_URANDOM_ARCH=y`. The existing Espressif driver registers this device with the same `esp_fill_random()` hardware implementation as `/dev/random`; no software PRNG, time seed, fixed data, or successful-error substitution was introduced. Controller initialization precedes host PRNG initialization. The 1042 ELF already contains actual LP_TRNG register reads in `esp_random()`, not the IDF bringup fixed-pattern branch.

The S31-only host diagnostics now report PRNG entry/result and final host initialization result so the board log distinguishes the corrected seed path from any remaining issue. Original bttool enable/disable commands, state criteria, and timeouts are unchanged. HAL and IDF are unchanged.

Independent output: `openvela-dev/out/esp32s31-xts-flat-bttool1055`.
Image size: 1210544 bytes.
Receipt: `build1055-flat-bttool-entropy.sha256`.
Build log: `logs/build1055-flat-bttool-entropy.log`.
Helpers: `build1055-bttool-entropy.sh`, `flash1055-bttool-entropy.sh`.

Build exited zero. Generated configuration selects architecture urandom and leaves XORSHIFT/congruential implementations disabled. Final map includes `devurandom_register`. Receipts for 1055, 1042, and 1031 verify. Scoped `git diff --check` and helper syntax checks passed. No UART access by this agent; root owns board validation. Build resource released after artifact verification.
