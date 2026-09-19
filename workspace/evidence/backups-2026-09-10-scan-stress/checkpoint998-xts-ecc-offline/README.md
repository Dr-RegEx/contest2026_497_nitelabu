# Candidate 998: Crypto without automatic association

BUILD ONLY; no target run, serial access, reset, flash or network setup was
performed for this candidate. The original eight Crypto applications and all
883 accelerator backends are retained. This profile inherits
`demo-rmt-xts-ecc` and enables `NETINIT_NETLOCAL` with automatic DHCP disabled.
The NSH initializer therefore performs only local interface setup; it cannot
call its network bring-up/association or NTP branch. Credentials were not
inspected or printed during this work.

Output: `openvela-dev/out/esp32s31-xts-ecc-offline`.
Receipt: `build998-xts-ecc-offline.sha256`, one kernel/AppFS ABI pair.
Keep `S31_DEMO_PROFILE=demo-rmt-xts-ecc-offline` for both build and flash tools.
The helper checks the offline flags, eight original applications and retained
accelerator options. The existing 883 output and receipt remain unchanged and
both original image hashes were reverified. This candidate must not be called
a target PASS until its original Crypto programs have run on the board.

Validation: F0 dependency verification; NuttX diff --check; shell syntax;
clean independent 3666-step build; existing Wi-Fi artifact checker; image
partition size checks; paired checksums; eight original Crypto app existence.
Configuration and logs are local reproducibility evidence, not public output.

Authorized root execution after exclusive UART ownership is available:

```sh
S31_DEMO_PROFILE=demo-rmt-xts-ecc-offline bash backups/2026-09-10-scan-stress/flash-demo-pair.sh "$PWD/backups/2026-09-10-scan-stress/build998-xts-ecc-offline.sha256"
```

Only the paired kernel (0x2000) and seed AppFS (0x200000) are written by this
entry. Writable `/apps` and the original xTS scratch range are not rewritten.
The original Crypto runner should retain its hardware-ecc backend selection.
