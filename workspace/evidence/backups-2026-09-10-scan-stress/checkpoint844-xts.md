# S31 xTS checkpoint — 2026-09-15

Priority: finish board adaptation and original published xTS within two days.
The user explicitly deferred external BMI160, GPIO jumpers and power-cycle
fixtures. Keep those cases pending, not N/A. No new repositories or reference
source changes. Existing demo and all earlier failures remain available.

## Verified additions

Common verified headings: **25/35 (71.4%)**, not total project completion.

- 839 on firmware837: original `mm`, `TEST COMPLETE`, no failed or skipped
  allocation, 6.758s. FLAT user heap includes the full 16MiB PSRAM region.
- 840 on837: `mkrd -m 10 -s 1000 1024`, then original
  `cmocka_driver_block -m /dev/ram10`, stress/single/cache tests3/3,2.364s.
  Only a fresh RAM disk was written; persistent Flash was not tested.
- 841 on837: original `cmocka_driver_rtc`, API/alarm/periodic3/3,37.343s.
  Absolute alarm readback, absolute and relative timing, cancellation and
  SIGEV_THREAD periodic delivery passed with the original tolerances.
- 844 on810: eight original Crypto applications pass. AES-CBC makes28 actual
  hardware requests totaling640bytes, all status0; the other seven use the
  established software backend. This is not full silicon crypto coverage.
- Supplemental NIST808, previously left marked running, actually completed:
  100streams,188numeric report rows, all P-values above0.0001,5372.011s.
  Published10-stream786 remains incomplete; no replacement of that result.

## Fixed boot blocker

Flash820 of816 succeeds, but821 stops during early startup. Temporary
instrumentation824/827/830/833 locates the first external heap metadata store.
The PMA candidate825 did not fix it. The module datasheet table1-1 specifies
Octal PSRAM; unlike production, `xts-flat` omitted
`CONFIG_ESPRESSIF_SPIRAM_USE_8LINE_MODE=y`. The locked HAL then enables16-line
data access in `esp_psram_impl_ap_hex.c`. Adding the correct8-line setting
allows original mm836 to complete. All temporary instrumentation and the
ineffective PMA candidate were removed before clean build837 and tests839–841.
The diagnostic source snapshots remain in `diagnostic836/`.

FLAT PSRAM registration reuses the HAL's actual free interval and wraps its
otherwise empty NuttX heap-registration hook. Kernel/MMU page allocation is
unchanged. RTC candidates from the previous session are now verified: UTC
deadlines,64-bit microsecond conversion and hardware HR-timer periodic events.
Periodic delivery here describes awake operation, not deep-sleep wakeup.

## Commands

Run from the workspace root; D denotes `backups/2026-09-10-scan-stress`.

```sh
S31_FLAT_PROFILE=xts-flat-rtc bash "$D/build-xts-flat.sh" "$PWD/$D/build837-flat-rtc-octal.sha256"
S31_FLAT_PROFILE=xts-flat-rtc bash "$D/flash-xts-flat.sh" "$PWD/$D/build837-flat-rtc-octal.sha256"
s31-reference/.venv-nuttx/bin/python -u "$D/xts-flat.py" heap
s31-reference/.venv-nuttx/bin/python -u "$D/xts-flat.py" ramblock --no-boot
s31-reference/.venv-nuttx/bin/python -u "$D/xts-flat.py" rtc --no-boot
bash "$D/flash-demo-pair.sh" "$PWD/$D/build810-xts-aes-cbc-proof.sha256"
s31-reference/.venv-nuttx/bin/python -u "$D/xts-crypto.py" --backend hardware-cbc
```

Use fresh receipt/log names for new builds. Original complete output resides
in logs837–844.843 was rejected by the UART exclusive lock while842 was still
finishing; no test command ran.844 ran after842 terminated successfully.
The paired flash script now rejects busy UARTs before doing anything.

`firmware837-flat-rtc-verified.tar.gz` and
`firmware810-hardware-cbc-verified.tar.gz` are hashed in `SHA256SUMS-xts844`.
`checkpoint841/` records repository heads, dirty state and tracked patches.
F0 dependencies verified unchanged. No original xTS test assertions changed.

## Long-duration blocker found next

845 flashes the preexisting KASAN candidate819 successfully.846 cannot boot:
an early sanitizer call targets unmapped Flash, with store/load hooks resolved
at0x40012210/0x400121fc. No12h/24h observation started. The logs and failed
state JSON remain.847 moves the KASAN runtime into IRAM and stops retained
KASAN state before BSS initialization; all normal kernel/HAL instrumentation
stays enabled. Follow the live status ledger for its target result.
