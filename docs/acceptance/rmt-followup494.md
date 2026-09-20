# RMT follow-up scope, after initial-acquisition fix

Source review: common/espressif/esp_rmt.c, esp_ws2812.c,
drivers/rmt/rmtchar.c and include/nuttx/rmt/rmt.h.

The two completion waits in rmt_write_items still ignore return values.
Simply returning on EINTR would release the WS2812 lock while hardware may
still use its buffer. Simply posting the semaphore can admit another writer
before completion. nxsem_wait_uninterruptible can still return ECANCELED.
Safe cancellation needs stop/IRQ acknowledgement/state serialization first.

tx_sem is currently both channel ownership token and TX-completion event.
The raw rmtchar upper half has no write mutex. Multiple writers can compete
for an ISR post intended for the active writer. Split ownership and completion
only together with audited TX end/loop/error paths and installation cleanup.
TX error ISR currently resets/acknowledges but does not wake with an error.

ISR threshold refill retains tx_data. WS2812 allocates its encoded frame in
kernel memory, but a raw userspace RMT write can provide a user virtual
address. Kernel-build long-frame refill therefore needs a kernel snapshot
whose lifetime extends through safe TX completion/cancellation. Short one-LED
tests fit hardware memory and do not validate this path.

Do not wrap the whole ISR with the existing global rmt_spinlock: refill
helpers also take it. A per-channel state lock needs documented lock order
and must serialize both stop and refill before releasing buffers. Duplicate
TX_DONE/loop/error status in one interrupt must not signal another transfer.

Future verification: actual extracted-function host models for competing
writers, callback-before-wait, completion/error collision and cancellation;
bounded long-frame board tests only after the pin/protocol scope is confirmed.
Current scope remains GPIO60 low-brightness onboard RGB, with final off.
