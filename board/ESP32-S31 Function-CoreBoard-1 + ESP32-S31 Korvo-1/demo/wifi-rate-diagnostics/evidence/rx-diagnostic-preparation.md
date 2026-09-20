# RX diagnosis preparation — offline only, 2026-09-20

This change prepares observation of the next UDP receive reproduction. It is not a throughput fix and does not establish a cause for the recorded collapse. No firmware build, board access, serial command, configuration change, or flash operation was performed for this preparation. Existing raw evidence remains unchanged.

## Evidence boundary

The final counters in `remaining-tx-udp/5.1.23-tx-board.log` and `5.1.24-rx-board.log` give the following interval deltas:

| Counter | Before | After | Delta |
|---|---:|---:|---:|
| WLAN RX frames | 0x2fc6 | 0x49c6 | 6656 |
| UDP received | 0x001f | 0x193e | 6431 |
| WLAN RX dropped | 0 | 0 | 0 |
| UDP dropped | 0 | 8 | 8 |

Current `wlan_rx_done()` increments WLAN RX dropped for admission, allocation, copy, and queue failures. These counters do not support hundreds of thousands of drops in that path. The host's 945256 transmitted datagrams are not evidence that those datagrams crossed the AP or reached the vendor receive callback. A callback entry count and upstream queue failures can narrow this boundary; packet capture or hardware counters are still needed to localize radio/AP loss.

## Prepared source changes

- `openvela-dev/nuttx/arch/risc-v/src/esp32c6/esp_wifi_adapter.c`: ESP32S31 plus NET_STATISTICS only; persistent slots for the first 16 successfully created queue lifetimes. Each slot records configured capacity/item size, active state, completed zero-wait send attempts and failures. Counters saturate at UINT32_MAX. Deleted slots remain observable and are never reused; later lifetimes are explicitly counted as untracked. Existing zero-wait send semantics and return values are unchanged. There is no packet-path logging or diagnostic heap allocation.
- `openvela-dev/nuttx/arch/risc-v/src/common/espressif/esp_wlan.c`: two minimal diagnostic additions under the existing ESP32S31 plus NET_STATISTICS guard: a cumulative callback entry counter and one relaxed atomic increment. It includes all station/SoftAP calls, including subsequent rejected packets. Counter differences use unsigned 32-bit arithmetic; the counter can wrap.
- `openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_monstat.c`: expose `WIFIRX: callbacks=...` and `WIFIQ` records via the existing `/dev/s31stat` reader. Shared text storage grows from 2048 to 4096 bytes; formatting stays in reader context. Per-counter atomic samples are not a transaction across queues or a frozen packet snapshot. Queue slots contain no live queue pointers, so deletion cannot invalidate a concurrent read.

`WIFIQ fail0` counts any negative zero-wait send result, not exclusively queue-full errors. A failure increase identifies an upstream adapter event delivery failure and needs correlation before attribution to RX. Queue `depth` is configured capacity, not current occupancy. `untracked > 0` means those later queues are outside the per-queue diagnostic coverage. Saturated counters cannot support further interval deltas.

Existing hardware RX statistics remain sampled at connection/stop by the existing adapter diagnostics. They were not added to monstat: this preparation does not establish that arbitrary concurrent vendor statistics API calls are safe during RX. A production RX hardware snapshot endpoint would need separate review.

## Host validation

Passed `git -C openvela-dev/nuttx diff --check`.

The reproducible host harness `tools/tests/wifi-queue-ownership/queue_diagnostics.py` extracts the actual new table, saturating increment helper, and formatter directly from the adapter source, and compiles with `cc -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined`. Checks passed:

- Production adapter/wrapper types are extracted too: the vendor-visible adapter layout and first-member offset are checked; an allocated private wrapper round-trips through the opaque handle, container recovery, and free under sanitizers.
- Four concurrent threads × 100000 increments yielded exactly 400000.
- UINT32_MAX - 1 saturated at UINT32_MAX without wrapping.
- All 16 queue slots, deleted/active state, maximum counter values, and two untracked lifetimes remained observable.
- Every output buffer length from 0 through 4095 returned a bounded length, NUL-terminated when space existed, and preserved the first byte outside the supplied range.

This is host validation of diagnostic concurrency/arithmetic/formatting only. It does not compile the firmware or validate the target ABI, linking, ISR cost, RF behavior, or throughput.

## Next reproduction interpretation

After a separately reviewed build and board deployment, capture `cat /dev/s31stat` and `ifconfig` immediately before and after one short bounded UDP RX run, preserving paired host/board logs and offered rate. Take baseline after association and avoid resets or interface cycling between snapshots.

- Low callback delta, no zero-wait failures, low RX frames: loss/delay is before the callback; investigate sender/AP/radio/vendor receive behavior.
- Rising `fail0`: investigate the identified queue's event delivery; do not equate every failed queue message with one missing UDP packet.
- High callbacks plus rising driver drops: inspect RX admission/IOB pressure.
- High callback/RX-frame deltas but low application reads: inspect protocol/socket drops and application sequence/accounting.

Capture after traffic has stopped as well, to distinguish delayed draining from permanent loss. The 300-second historical results and pass counts must not be rewritten by this diagnostic candidate.

Exact incremental source patch excluding pre-existing edits: [rx-diagnostic-only.patch](rx-diagnostic-only.patch), reconstructed by reversing only the additions listed above; reverse application check passed. This preparation did not run firmware target compilation.

Reproduce from the workspace root with `python3 tools/tests/wifi-queue-ownership/queue_diagnostics.py`. The actual successful rerun, exact compiler command, production-source hash, and extracted-helper hash are preserved in [rx-diagnostic-host-test.log](rx-diagnostic-host-test.log). The test extracts current production source on every execution and uses temporary files only for the host compilation outputs.


## Target integration correction

The root agent's first target compile rejected adding a field directly to `struct mq_adpt`: the target vendor `platform/os.h` also defines that structure. The initial helper-only host test could not detect this integration constraint. The corrected candidate preserves the original `mq_adpt` declaration and uses a private `s31_wifi_mq_adpt` wrapper whose first member is the original structure. Only the local `xqueue_create_adapter()` allocation is enlarged; normal and Wi-Fi-static queue creation hooks both route through that allocator. Send/delete recover the private wrapper with `container_of`; no vendor structure or vendor file is modified. Because the original adapter is the first member, the opaque handle remains the allocation base and existing frees remain valid.

The persistent patch and host test log have been updated for the corrected source. The host harness now also extracts both production structure declarations and checks head layout, wrapper recovery, and allocation/free. Firmware rebuild status belongs to the root agent's separate build evidence, not to this host test.
