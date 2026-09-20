# AES CTR/XTS and parameter fallback prepared — not yet board-verified

Longrun864 continues on firmware860; no UART access or reset occurred here.

Optional `ESP32S31_CRYPTO_AES_MODES` extends the existing CBC PIO engine to
full-block CTR/XTS and streaming requests. CTR uses cryptodev's RFC3686
low32-bit counter semantics. XTS encrypts the data-unit IV with key2 once,
then retains and advances its little-endian GF tweak across updates. Both
data and tweak AES operations use the same hardware primitive. Session keys
are volatile and erased on close; no eFuse or Flash-encryption keys are used.
CTR/XTS do not write caller IV buffers, matching the original software API.

The hardware accepts128/256-bit AES and256/512-bit combined XTS keys. The
original CTR vectors also contain AES192, which S31 does not support.
Source review found the existing framework chose a driver by algorithm but
never retried after it rejected session parameters. The selector now tries
remaining drivers on EINVAL/ENOTSUP, in its existing hardware/remote/software
order. Hardware-only requests stay hardware-only; resource/device failures
are returned directly rather than hidden by fallback.

-869: separate `demo-rmt-xts-aes-modes` buildPASS.
-870: actual mode code + original vector tables hostPASS.
-871: mode code plus framework fallback buildPASS.
-872: selector behavior and CBC4/CTR6/XTS14 original vectorsPASS;96 single/
 stream encrypt/decrypt comparisons,1568 ECB calls;3AES192 hardware rejections.
 Host address/undefined-behavior sanitizers report no errors.

`check-aes-modes-host.py` includes the actual driver source and actual selector
function, imports original vector tables, and substitutes OpenSSL ECB only
for the hardware primitive. It does not establish S31 peripheral operation,
locking, interrupts or timing. Hardware status remains NOTRUN for CTR/XTS.
CBC's previous844 hardware evidence remains valid for archived810.

After864 completes and releases UART:

```sh
S31_DEMO_PROFILE=demo-rmt-xts-aes-modes bash backups/2026-09-10-scan-stress/flash-demo-pair.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build871-aes-fallback.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-crypto.py --backend hardware-aes
```

Use fresh numbered logs. All eight original apps must pass, and CBC/CTR/XTS
must each produce successful hardware completion records. AES192 stays
software. Other crypto algorithms have not gained hardware coverage here.

`firmware871-aes-modes-build-only.tar.gz`, `SHA256SUMS-xts872` and
`checkpoint872/` preserve paired firmware, config, symbols and current source
state, including preexisting work. Main demo810 and longrun860 build outputs
were not overwritten.
