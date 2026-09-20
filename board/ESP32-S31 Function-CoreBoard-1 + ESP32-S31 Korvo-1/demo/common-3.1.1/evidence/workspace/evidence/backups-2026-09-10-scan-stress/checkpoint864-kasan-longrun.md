# S31 checkpoint: kernel KASAN and original long-duration xTS

Date: 2026-09-15. User priority remains board-only adaptation within two days;
external fixtures are deferred. Common verified coverage remains **25/35**.

## Actual target results

- Build860 and paired flash861: PASS, both written hashes verified.
- Original `cmocka_mm_test`862: **8/8 PASS**, 4.921s.
- Original `cmocka_syscall_test`863: **83/83 PASS**, 14.858s. Expected negative
  syscall diagnostics are part of the unchanged test, not sanitizer faults.
- Longrun864: started **2026-09-15 16:43:12 +08:00**, PID **144782**.
  Initial date error is conservatively bounded by [-0.594, +0.425] seconds.
  Both long-duration cases remain RUNNING, not PASS.
- 12h standby result is due **2026-09-16 04:43:12 +08:00**; 24h date comparison
  is due **2026-09-16 16:43:12 +08:00**. Comparison samples occur every6h.

Live state: `xts864-longrun.json`. Raw target evidence:
`logs/xts864-longrun-uart.log`; host verdicts:
`logs/xts864-longrun-host.log`. The host process owns UART exclusively.
Do not reset, flash or start another UART tool during this observation.
Keep the host awake and the board powered/attached for the full interval.

## Necessary fixes and preserved failures

1.846: sanitizer hooks were in Flash before Simple Boot mapped it. Link the
   runtime into IRAM and clear its retained marker before BSS initialization.
2.849: CMake's split `kmm` did not inherit `mm`'s no-sanitize compiler flags.
   Apply the existing Make allocator/checker exception to both targets.
   Normal kernel and HAL callers retain instrumentation.851 link inspection
   finds no recursive checker references and confirms instrumented callers.
3.853: the large PSRAM filesystem heap puts shadow descriptors in memory
   unavailable during external-cache suspension. The standby profile now
   inherits the RNG/driver profile directly, using the normal internal
   filesystem allocator, not the4MiB file-pressure-test heap. SMP, MMU and
   the16MiB PSRAM process page pool remain enabled. Large-PSRAM-FS-heap KASAN
   compatibility remains unresolved; it is not silently claimed as working.
4.856: `dispatch_syscall` pinned arguments to caller-saved registers, which
   inserted KASAN calls overwrite. Use normal C argument passing. Preserved
   before/after disassembly856/857 shows the missing argument saves restored.
5.859: a process-private heap shadow appeared in the global kernel registry;
   another CPU/address environment faulted on its virtual address. Set the
   existing `mm_heap_config_s.nokasan` option for BUILD_KERNEL process heaps.
   Kernel KASAN remains active; user-process sanitizer coverage is not claimed.

The original xTS assertions and workloads were not changed.846/849 failed
longrun JSON and853/856/859 boot failures remain available; none contributed
elapsed time to864.

## Observation method and limits

`xts-longrun.py` validates the paired receipt and required configuration,
boots once, takes wlan0 down, confirms no address, and sets UTC once from the
host. NTP is disabled. Existing `showinfo -i 60` supplies a heartbeat; missing
heartbeats, resets, crashes or sanitizer reports fail the observation.
No automatic retries or resets occur. In BUILD_KERNEL, showinfo's mallinfo
describes its own heap; standard `free`/`ps` snapshots additionally record
kernel and page resources at the comparison checkpoints.

The host bounds each second-resolution date reading between command send and
prompt receipt. The final conservative error interval must lie within ±2s.
No early PASS or shortened-duration substitute is used.

`firmware860-kasan-boot-verified.tar.gz` includes the matched kernel/AppFS,
config and symbols. `SHA256SUMS-xts864` records it and source checkpoints.
`checkpoint864/` contains repository heads/status, tracked NuttX delta and
untracked port sources; preexisting work is preserved. Archive837 retains
the verified FLAT memory/block/RTC image; archive810 retains hardware-CBC
and the LAN demo. See `checkpoint844-xts.md` for those functional results.
