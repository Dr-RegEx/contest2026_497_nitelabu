# ECDSA verification candidate — no target verdict

881 builds `demo-rmt-xts-ecdsa` in the independent
`openvela-dev/out/esp32s31-xts-ecdsa` output. It includes the prior AES modes,
SHA, SHA-accelerated HMAC and ECC random-buffer correction.

The new optional backend registers only `CRK_ECDSA_SECP256R1_VERIFY`.
It accepts the current original cryptodev affine-public-key ABI, reverses
five256-bit parameters into the locked LL's little-endian representation,
and selects ordinary-public-key verification with caller-provided digest.
Parameter2 is reserved by the existing ABI; the original software key
generator does not populate it. No eFuse key selector or signing mode runs.

The ECDSA/ECC lock serializes peripheral access. Both peripheral clocks and
ECC memory are enabled. The LL's unbounded ECDSA reset wait is replaced by
the same register reset followed by a bounded1s wait with interrupts enabled.
State transitions IDLE/LOAD/IDLE and signature-result status are checked;
nonzero status rejects signatures. Public-parameter buffers are cleared.
This does not accelerate key generation or signing, and does not establish
complete hardware ECDSA coverage.

Source review found `crypto_kinvoke` always selected the first registered
implementation. Because software registers first, it hid the hardware driver.
The selector now tries supported hardware first, then software only when
allowed; operation indexes are checked before accessing the algorithm table.
It does not silently retry invalid signatures or failed hardware operations.

Build881PASS. Host882 includes the actual adapter and actual selector,
substituting OpenSSL only for peripheral verification. Valid/invalid signatures,
incorrect parameter length, bounded timeout, hardware preference, allowed
software fallback and out-of-range operation rejection pass. ASAN/UBSAN clean.
The digest is imported from the original xTS ECDSA example. No original test
source or reference source changed. Register timing/clocks remain unverified.

After longrun864 releases UART, capture fresh numbered logs:

```sh
S31_DEMO_PROFILE=demo-rmt-xts-ecdsa bash backups/2026-09-10-scan-stress/flash-demo-pair.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build881-ecdsa-verify.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-crypto.py --backend hardware-ecdsa
```

All8original apps and successful AES/SHA/HMAC/P256-verify hardware records are
required by this batch. Original ECDSA keygen/sign remain software. Earlier879,
878 and871 outputs are retained for isolation after an actual target failure.

Archive: `firmware881-ecdsa-verify-build-only.tar.gz`, `SHA256SUMS-xts882`.
Source: `checkpoint882/` including preexisting work. Common xTS stays25/35;
longrun864 remains running, not a12h/24h PASS.
