# ES8311 kernel worker prerequisite

Status: local RV32 object compilation verified; no firmware link or target run.
Only `nuttx/drivers/audio/es8311.c` and its private `es8311.h` changed.
Original files are saved as `*.before`; `worker.patch` is relative to them.

## Change

BUILD_KERNEL starts a privileged `kthread_create` worker, following the existing
`drivers/audio/audio_fake.c` copied-argv pointer transport. Its wrapper calls
the existing codec loop without changing buffer processing. A per-instance
completion semaphore is posted only after the loop has returned: pending/done
buffers, queue close/unlink and COMPLETE callback are already finished. The
wrapper does not access codec state after posting. Normal task exit reclaims
the kernel task/stack; STOP waits for driver cleanup, not a pthread join or a
forced task deletion. The device state remains board-lifetime allocated.

START consumes any prior natural completion before opening a new queue, rejects
an active worker, and publishes `running` before task creation. Failed creation
clears task/running state, closes/unlinks the queue, and releases the existing
shared codec/I2S lifecycle where enabled. No completion is awaited after a
failed create. STOP and RELEASE consume one completion per successful start;
the task ID is cleared by the existing join callers. Kernel STOP checks failed
STOP-message delivery instead of waiting forever for a live worker that was
never notified. Nonshared kernel STOP handles absent/already-finished workers;
RELEASE requests STOP before waiting for an active worker. Audio upper-half
ioctls serialize lifecycle calls with `audio_upperhalf_s.lock`.

FLAT retains pthread creation, naming and join behavior. The worker body and
all APB handling remain unchanged. Kconfig safety guards remain in force.

## Checks

`python3 check.py` uses the existing RV32 compiler and saved compile command.
It compiles only this translation unit to this evidence directory:

- 1057 actual FLAT configuration: PASS.
- 1078 actual BUILD_KERNEL/SMP/Sv32 config plus existing 1057 audio-driver
  symbols: PASS (not a new integrated profile or firmware).
- Same kernel configuration without shared codec option: PASS.
- FLAT preprocessed source before/after is identical after normalizing source
  paths/assert line numbers, whitespace, and harmless `(priv)->threadid` macro
  parentheses. No behavior differences remain in that comparison.
- `git -C openvela-dev/nuttx diff --check`: PASS.

Compiler logs retain pre-existing codec uint32 format warnings exposed by the
kernel configuration's logging. No warning suppression or unrelated fixes.

## Limits

This is only the worker-privilege prerequisite. APB address-environment retention,
user-pointer access/callback/free safety and process-exit cancellation remain
unresolved by design. It does not make the kernel audio integration runnable or
validated. No Kconfig guard was relaxed, HAL/IDF touched, firmware built, or
UART/Flash accessed. Frozen passing images and receipts were not overwritten.

The completion assumes normal worker exit. It cannot recover a wedged I2S
provider that never returns inflight buffers, a nonreturning upper callback,
or forced external kernel-task deletion. These were not hidden with timeout
success or forced teardown. Hardware STOP/drain, restart, real kernel memory
headroom and integrated audio listening still require target validation after
the separate APB contract is implemented.
