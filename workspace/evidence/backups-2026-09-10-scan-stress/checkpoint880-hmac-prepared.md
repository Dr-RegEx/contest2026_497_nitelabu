# SHA-accelerated HMAC candidate — target pending

879 builds `demo-rmt-xts-hmac` in its own
`openvela-dev/out/esp32s31-xts-hmac` output. It includes the AES modes, SHA
backend and four-line ECC random-buffer correction from878.

Optional `ESP32S31_CRYPTO_HMAC` computes HMAC-SHA1/256 inner and outer hashes
using the SHA PIO engine. Volatile keys up to64bytes are held as padded
session state, then erased on finalization/free. Longer keys are rejected
with ENOTSUP so the existing selector tries the software backend. HMAC-MD5
stays software. This is not the dedicated eFuse-keyed HMAC peripheral, and
no permanent key provisioning is involved.

Host880 imports the original6 HMAC-SHA vectors without changing the test
source, checks each in single/split input, and verifies long-key rejection.
The12original SHA vectors and2interleaved sessions also pass:26 comparisons,
63131 OpenSSL compression calls, ASAN/UBSAN clean. Only host compression
substitutes the peripheral; S31 hardware operation remains unverified.

After longrun864 finishes and releases UART, capture fresh numbered logs:

```sh
S31_DEMO_PROFILE=demo-rmt-xts-hmac bash backups/2026-09-10-scan-stress/flash-demo-pair.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build879-hmac.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-crypto.py --backend hardware-hmac
```

The runner requires all8original applications to pass, actual CBC/CTR/XTS
completions, all3 SHA large-input completions and each HMAC-SHA engine's
8/28/50-byte completion records. Whole Crypto hardware acceptance remains
incomplete: ECDSA is still software, and dedicated keyed peripherals have
not been verified. Earlier SHA-only878 and AES-only871 outputs are preserved
for isolation if this candidate fails on hardware.

Archive: `firmware879-hmac-build-only.tar.gz`, `SHA256SUMS-xts880`.
Source snapshot: `checkpoint880/`, including preexisting uncommitted work.
No new common xTS heading is counted;25/35 stays unchanged. Longrun864 has
passed its first hour of observation, but neither12h nor24h is complete.
