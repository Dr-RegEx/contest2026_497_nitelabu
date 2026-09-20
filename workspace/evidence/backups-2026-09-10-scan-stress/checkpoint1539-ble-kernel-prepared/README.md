# SMP/MMU BLE integration candidate 1539

Build and paired packaging passed; no target claim in this preparation archive.

The controller runs in a persistent CPU0 kernel dispatcher. Kernel OSAL tasks pin to CPU0 before entering controller code. HCI send payloads are copied into kernel-owned requests. FLAT behavior remains on the existing direct path.

The userspace Zblue port uses monotonic clock_gettime, a recursive host mutex for virtual IRQ regions, ordinary userspace sleep, and cancellable user work-queue timers. Timer callback tickets survive work-item cancellation when the user worker has already dequeued them, and serialize callback delivery/cancellation with the host lock. BR link-mode events are guarded in BLE-only builds.

Wi-Fi coexistence and SIMD remain off. The target test must establish boot, host startup, GATT reads/write, disable and teardown before integration acceptance. 1538 link failure is retained. The userspace timer implementation still needs target exercise; compile success is not acceptance.
