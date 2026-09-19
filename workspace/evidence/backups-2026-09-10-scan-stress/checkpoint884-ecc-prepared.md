# Combined Crypto candidate with ECC point acceleration

883 builds `demo-rmt-xts-ecc` in independent `out/esp32s31-xts-ecc`.
It includes881's hardware P256 verification and prior AES/SHA/HMAC changes.
No board was reset or flashed during the build; longrun864 continues.

An optional kernel ECC hook accelerates unblinded affine P256 multiplication,
which the current original keygen/sign implementation uses. Big-endian public
coordinates and volatile scalar are converted to the locked LL's little-endian
parameter buffers. The driver selects verify-then-point-multiply and enables
hardware constant-time mode, shares the ECC lock with ECDSA, bounds the wait
to1s and clears all six48-byte parameter-register banks afterward. Hardware
failure gives the caller a zero point and its existing bounded failure path;
there is no silent software retry. Signature scalar arithmetic remains in the
existing library. Projective-blinded ECDH and other curves stay software.

Host884 includes the actual library and actual driver, replacing only the
point primitive with OpenSSL. Keygen/sign integration makes5point calls in
each native/no-int128 build; signatures verify, changed hashes reject, ECDH
matches, seven RNG requests each fill the complete32-byte buffers, and all
parameter-register shim bytes are cleared. ASAN/UBSAN clean. These checks do
not establish actual hardware constant-time behavior, timing or register I/O.

After864 completes and releases UART, use fresh numbered logs:

```sh
S31_DEMO_PROFILE=demo-rmt-xts-ecc bash backups/2026-09-10-scan-stress/flash-demo-pair.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build883-ecc-point.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-crypto.py --backend hardware-ecc
```

Require all8 original apps, successful CBC/CTR/XTS, SHA, SHA-accelerated HMAC,
P256 verification and at least2 ECC point completion records. AES192, MD5,
HMAC-MD5 and3DES stay software; CRC remains the established implementation.
This is a hybrid ECDSA implementation, not eFuse-keyed signing. No permanent
keys or protected storage are used. If the combined candidate fails, retain
the complete failure log before isolating with the archived earlier images.

Artifacts: `firmware883-ecc-build-only.tar.gz`, `SHA256SUMS-xts884`.
Source snapshot: `checkpoint884/`, preserving prior uncommitted work too.
All new Crypto hardware paths remain TARGET NOTRUN. Common xTS remains25/35.
