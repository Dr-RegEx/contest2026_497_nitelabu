# Original KVDB target sequence (candidate948, not executed)

Run only after864 has completed, the blank1MiB scratch backup is verified,
and common946 raw Flash tests pass. Preserve earlier filesystem evidence
before changing its contents. No command below addresses production /apps.
Flash the receipt-checked948 FLAT image using the existing helper.

On fresh boot, inspect `mount`, `ps`, `ls /dev/xtsflash` and the boot marker
confirming offset0xc00000 size0x100000. Board bringup already mounts /tmp as
TMPFS; do not mount another filesystem over it. /data must be the isolated
scratch LittleFS. Use `mkdir /data` if absent. For the first scratch formatting
only, after the prerequisite evidence is checked:

```text
mount -t littlefs -o forceformat /dev/xtsflash /data
```

Every later mount, including recovery and firmware changes, must omit
forceformat/autoformat:

```text
mount -t littlefs /dev/xtsflash /data
```

For4.1.11, start the original daemon in the background, confirm it remains
in `ps`, and execute the original suite:

```text
kvdbd &
ps
cmocka_kv_test
```

Require30 actually executed and passed original cases, zero failed/skipped,
no fatal markers. The original cmocka main returns0 unconditionally after
cmocka_run_group_tests, so shell exit status alone cannot establish PASS.
Retain the full per-case output and final cmocka summary.

For5.1.68, reboot first so no daemon holds persist.db open. Mount /data without
formatting and verify /tmp is mounted. Delete only /data/persist.db if present,
then issue the published reboot. Preserve all other filesystem evidence.
After that reboot, remount the same /data without formatting, start kvdbd,
confirm the daemon in ps, and run:

```text
vela_kvdb_stability_test02 10
```

Keep10 rounds, allow at least30minutes host command timeout (published estimate
is12minutes, not a deadline). Original source prints `TEST PASSED !` including
a space before !, and handles ENOSPC separately. Retain any ENOSPC occurrences
and context; do not silently drop them or interpret every `store FAILED` as
a crash. Acceptance requires the original final verdict and no target fatal
error. Do not auto-retry or reset a running test on a short host timeout.

Configuration and entrypoints are already compiled in948; all of the above
remains TARGET NOT RUN. This is execution preparation, not new test coverage.

## Prepared original-suite transport (not executed)

`xts-kvdb-suite.py --receipt build948-flat-category-kv.sha256
--mount-evidence <current-boot-mount-transcript>` runs only4.1.11, once. Use
the existing project Python environment with pyserial and capture stdout/stderr
to a new result log. The transcript must identify the receipt using
`XTS_RECEIPT=<absolute receipt path>` or its image digest, contain the successful
explicit scratch mount command and a subsequent `mount` listing showing
`/data type littlefs`. Commands use existing `XTS_COMMAND=`, `--- CMD: ... ---`,
or NSH prompt log conventions. Later reset, unmount or remount is rejected.
The prior authorized first `forceformat` mount may appear in this evidence;
the runner itself never formats, mounts, deletes, flashes or resets anything.

NuttX mount output does not expose the source device. Source identity therefore
comes from that current-boot command evidence together with live mount,
`ls /dev/xtsflash`, and process checks, not from an invented mount field.
The runner refuses an existing kvdbd or any /apps mount. It starts kvdbd,
checks it remains running, then requires all original30 RUN/OK lines and the
30-case summary with no failures/skips. It leaves the daemon and data intact.
The runner uses `os.open` on the existing raw115200 tty with an exclusive
advisory lock, without DTR/RTS or termios changes. Opening a tty can still have
driver-dependent effects: boot output or missing existing mounts causes
refusal, without automatic recovery. This tool must not be run
during the active long test, even if its UART owner is temporarily absent.
