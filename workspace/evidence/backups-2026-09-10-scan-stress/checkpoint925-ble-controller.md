# BLE controller candidate 925 — BUILD ONLY

Image packaging succeeded: 811484 bytes. All 54 pinned OSAL header wrappers have strong definitions in libarch.a; 37 are retained by the current controller ELF, unused wrappers are garbage-collected. Dedicated controller sections now follow the pinned awake configuration instead of becoming orphan image segments.

No target execution, advertising, scanning or pairing PASS. An OSAL interrupt-context semaphore issue and cache-off call closure are under review before the next candidate. This frozen image is build evidence, not a flash recommendation. Original longrun 864 retains the UART.
