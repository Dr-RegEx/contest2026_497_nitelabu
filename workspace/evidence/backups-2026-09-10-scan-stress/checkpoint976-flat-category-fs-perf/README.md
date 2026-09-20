# Filesystem performance/stability976 — BUILD ONLY

419356-byte clean rebuild of fs-perf, keeping existing950 archived and redirecting
its receipt to that frozen binary. All1392 previous build outputs were cleaned
before the rebuild. Same1MiB scratch, PSRAM heap/Flash dispatcher and testcase
priority255. No automatic mount, format, Wi-Fi or hardware execution.

Necessary test01 correction joins successfully created workers before checking
results, closing shared FILE and cleanup, including partial-create failure.
Exit flags are cleared before creating workers. Three worker bodies, workload
and minute-based duration remain unchanged. The test04 reader correction from973
is also included. Both original sources and patches are archived. Results from
these corrected cases must explicitly disclose PATCHED TEST SOURCE.

Host ASan/UBSan real-thread regression reproduced original close-with-active-
workers failure and verified corrected normal/partial-create-failure shutdown.
Host-only harness forces immediate end-of-duration to exercise shutdown; it is
not a shortened target xTS run. No board test was performed. Build and guards,
source whitespace and independent source review passed. Source base970 plus
these patches reproduces the source changes; use the preserved profile/helper.

Use976 for short/performance and timed stability preparation. test01's actual
-t units are minutes: -t60 for the published1h example, or -t720 only if12h is
chosen. Preserve this difference and actual elapsed time. Use separate973 for
automatic crash-reset cases and974 for disclosed internal-SRAM original5.1.3.
See fs-short-target-sequence.md and fs-recovery-target-sequence.md.
