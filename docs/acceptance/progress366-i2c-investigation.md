# I2C investigation through build 366

This is a work-in-progress record, not an acceptance report.

- Build 355 (`demo-i2c-cpu1-init`) completed successfully; flash 356 verified
  the matching kernel/AppFS hashes. Demo 357 connected, HTTP 358 passed all
  100 samples and boundaries, I2C 359 passed ten batches (960 ID reads and
  30 controller recoveries).
- Build 360 returned to `demo-i2c`, CPUSET 3, with a task/ISR device spinlock
  and timeout quiesce/semaphore cleanup. Host 361 passed. Flash 362 verified;
  demo 363 and HTTP 364 (100 samples plus boundaries) passed.
- I2C 365 passed six complete batches, then failed batch 7 on the final
  read in the first 100 kHz FE-register group. The first real error was
  ETIMEDOUT (110), state FINISH, raw=0x202, enabled=0x5a8, error=0,
  ctr=0x10, sr=0x6600c001, command0=0x1000, command1=0x2000.
  Command 0 is STOP, not WRITE. The i2ctool can issue the pointer write and
  data read as separate transfer calls, so message index 0 alone does not
  identify the pointer-write phase. Earlier inference to that effect was
  incorrect. The controller was idle without command-DONE at the snapshot.
- The device lock did not resolve the failure. It remains necessary
  exclusion for shared task/ISR state, not a proven root-cause fix.
- Build 366 preserves newly arriving peripheral events during command
  rearming. Old S31 paths cleared all events in intr_enable and the HAL
  TX/RX interrupt helpers, potentially hiding an error between commands.
  Clearing is now restricted to message boundaries and ISR captured status.
  Build 366 succeeded, flash 367 is the next hardware experiment.
- Flash 367 and demo 368 succeeded. I2C 370 failed on the first FD read
  during concurrent HTTP 369: still STOP-stage ETIMEDOUT, now raw=0x8252.
  This exposes MST_TXFIFO_UDF (bit 6), BYTE_TRANS_DONE (bit 4), DET_START
  (bit 15), plus bits 9 and 1. No masked timeout/NACK/arbitration error was
  present. Preserving events did not resolve the failure. The next useful
  diagnostic is the bounded sequence of ISR status/command snapshots.

Full commands are traced in each build/flash log. Build command form:

```sh
S31_DEMO_PROFILE=demo-i2c bash backups/2026-09-10-scan-stress/build-demo.sh /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build366-i2c-events.sha256 > backups/2026-09-10-scan-stress/logs/build366-i2c-events.log 2>&1
```

No software wait timeout was increased. No reference repository, audio
configuration, PA output, efuse or flash region at/above 0x500000 was changed.
The I2C source group remains uncommitted pending hardware investigation.
NuttX HEAD is 9e8cae06cf5; Apps HEAD is e498e0ec6.

Do not treat a successful CPU1 run as proof of cross-core causality.
CPU placement also changes interrupt latency and execution timing.

## Follow-up: trace 376 and kernel buffer candidate 377

Build 372/flash 373/demo 374 succeeded. I2C 376 passed one batch, then
failed in batch 2 during HTTP 375. The bounded trace captured:

```
irq[0] state=1 status=8 raw=821a cmd=80003000/80000901 flags=0
irq[1] state=2 status=80 raw=82d2 cmd=00000900/00002000 flags=0
```

The first event followed address/restart completion. The ISR saw flags 0
and generated a zero-length WRITE (0x900), despite i2ctool's register-pointer
write descriptor specifying NOSTOP (0x40), length 1. Thus the earlier
separate-read explanation of message index 0 is not established; inspecting
only the eventual STOP command was insufficient. The sequence points to
invalid descriptor visibility, not merely a delayed STOP interrupt.

This kernel build enables ARCH_ADDRENV. The I2C upper half forwards user
message pointers unchanged; the ISR dereferences them after the task sleeps,
possibly on another hart/in another user mapping. A device lock cannot fix
address-environment identity. Candidate 377 snapshots message descriptors
and payloads into kernel allocations for S31 ADDRENV builds, then copies
successful read payloads back in the issuing task. It also bounds message
indices, lengths and aggregate size and cleans up both allocation failures.

Build 377 and host 378 passed. The host model mutates the original descriptors
during the lower-half call to check that transfer and copy-back rely on the
kernel snapshot. Hardware validation of 377 is still pending at this note.
