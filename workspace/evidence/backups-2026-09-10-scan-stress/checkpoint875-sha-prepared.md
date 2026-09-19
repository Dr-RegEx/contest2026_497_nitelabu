# Hardware SHA candidate — build and host checked only

873 builds `demo-rmt-xts-sha` in the independent
`openvela-dev/out/esp32s31-xts-sha` directory. It inherits the AES modes
candidate and adds SHA1/SHA256/SHA512 cryptodev sessions. The pinned HAL's
PIO interface supplies compression, mode selection and digest save/restore.
Input is copied into aligned session SRAM blocks; no DMA or direct peripheral
read of private user pages. Session locks protect close/stream state; the
existing shared AES/SHA lock serializes hardware ownership. Polling has a
20ms tick-based timeout, and failed/finalized sessions reject further use.

The backend follows the current original hash test's update/final API.
It rejects keyed/chained sessions. MD5 and HMAC retain software backends.
Digest register bytes follow the locked reference's PIO representation;
this still requires target confirmation. SHA160 completion markers denote
SHA-1's160-bit digest, not another algorithm.

Host874 passes the9 ordinary original SHA vectors. Configuration review
confirms600KiB HASH_HUGE_BLOCK is enabled, so875 also imports those3 original
expected digests. Total12original vectors plus2interleaved-session checks
pass,63083 compression calls; ASAN/UBSAN clean. The host shim uses OpenSSL
compression transforms with raw state save/restore. It validates buffering,
padding and streaming, not actual S31 peripheral behavior or concurrency.
No original xTS source was changed.

After864 releases UART, use a fresh log for each command:

```sh
S31_DEMO_PROFILE=demo-rmt-xts-sha bash backups/2026-09-10-scan-stress/flash-demo-pair.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build878-sha-ecc-random.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-crypto.py --backend hardware-sha
```

The runner requires all8original apps to pass, CBC/CTR/XTS hardware completion
records and SHA160/256/512 final records including each million-byte and
614400-byte case. MD5 remains software even when the aggregate hash app passes.
This profile can validate the queued AES changes in the same target batch;
separate871 remains available if isolation is needed after an actual failure.

`firmware873-sha-build-only.tar.gz` and `SHA256SUMS-xts875` archive the paired
images/config/symbols. `checkpoint875/` preserves current tracked delta and
untracked port sources, including prior work. Common xTS remains25/35; target
CTR/XTS/SHA are pending. Longrun864 was not reset or accessed here.

878 supersedes873 in the live SHA build directory after the four-line ECC
random-buffer fix. Host876 reproduces the old4-byte request;877 passes
full32-byte requests and P-256 keygen/sign/verify/rejection/ECDH in native
and no-int128 arithmetic. SHA source/config unchanged. Both873 and878
archives are retained; use receipt878 for the current output. Target pending.
