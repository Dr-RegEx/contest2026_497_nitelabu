# Actual duplex source host logic review

The harness extracts the current driver structs and dup_service, dup_cancel,
and dup_stop_dma bodies verbatim. source-manifest.json records the source hash;
actual-fragments.c preserves exactly the reviewed fragments. Hardware LL calls,
descriptor type, clock and audio-buffer wrappers are host stand-ins. The test
simulates descriptor CPU ownership and calls the actual extracted functions.

Validated with:

```
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra duplex-source-logic.c -o /tmp/s31-duplex-source-review
/tmp/s31-duplex-source-review
```

Result: three RX and three TX requests, 4096 bytes each; all RX byte ordering,
TX curbyte offset and ring payload ordering, final completion counts; STOP for
either direction leaves the other active, clears stopped slot references, and
retains RX master state until both stop. ASan/UBSan reported no errors.

This is implementation regression evidence, not an added xTS case or board
PASS. It does not exercise real DMA/registers, ISR concurrency, codec clocks,
worker callback dispatch or real timing. Tests do not access UART or network.
