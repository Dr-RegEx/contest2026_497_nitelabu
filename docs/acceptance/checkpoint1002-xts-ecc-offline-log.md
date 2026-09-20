# Candidate 1002: SHA/HMAC hardware completion logging

Build-only candidate; no UART, reset, flash, provisioning or target execution
was performed by this change. The independent profile
`demo-rmt-xts-ecc-offline-log` inherits the offline Crypto profile from998.
Output is `openvela-dev/out/esp32s31-xts-ecc-offline-log` and the paired receipt
is `build1002-xts-ecc-offline-log.sha256`. Both998 original image hashes still
match their receipt; its output and the1001 target evidence are preserved.

Original Crypto run1001 passed every cmocka functional result, but SHA/HMAC
completion messages showed bytes=0 and an unrelated positive status. Both
messages used a 64-bit variadic byte counter before the status argument.
998 has CONFIG_LIBC_LONG_LONG=y; absence of that option is not the cause.
Call-site disassembly supplies the expected registers. A runtime alignment or
variadic logging-path mismatch is consistent with the observed shifted values;
the exact underlying global ABI cause is not established by this local fix.

The only backend change converts the real uint64_t byte count into a decimal
string and logs that string with %s, while logging the real backend return
value using %d. All20 decimal digits fit, including UINT64_MAX. There is no
counter truncation, fabricated status, cryptographic algorithm change or
original test/runner relaxation. Existing session cleanup is unchanged.

Validation: F0 lock verification; NuttX diff --check; shell syntax; clean
independent3666-step build; existing Wi-Fi artifact check; offline configuration
and eight original Crypto app/backend guards; image sizes and paired hashes.
This compilation does not by itself establish that the target log defect is
fixed. Root will rerun the same original Crypto programs and preserve results.

Flash entry when root owns UART:

```sh
S31_DEMO_PROFILE=demo-rmt-xts-ecc-offline-log bash backups/2026-09-10-scan-stress/flash-demo-pair.sh "$PWD/backups/2026-09-10-scan-stress/build1002-xts-ecc-offline-log.sha256"
```

Use the existing original Crypto runner with its hardware-ecc backend. No new
acceptance tests or hardware requirements are introduced by this candidate.
