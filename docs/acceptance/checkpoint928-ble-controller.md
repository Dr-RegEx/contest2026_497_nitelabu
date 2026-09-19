# BLE controller candidate 928 — BUILD ONLY

Image: 811016 bytes. Replaces candidate 925 for further integration.

Fixed ISR zero-time semaphore acquisition using NuttX nxsem_trywait_slow on the OSAL SEM_PRIO_NONE counting semaphore; nonzero ISR waits remain rejected. Mapped the identified normal cache-off semaphore/watchdog/scheduler/FPU call closure to internal RAM and placed the OSAL random wrapper there. ELF verification confirmed the inspected functions are in 0x2f... memory. This is a bounded source/ELF review, not proof of every possible controller path or fault-reporting path.

No target boot, HCI initialization, advertising, scanning, pairing or category PASS. Longrun 864 remains uninterrupted. Original bttool/Zblue integration is separately building as 929 after BLE-only ATT guards and disabling optional GATT database caching in the initial profile.
