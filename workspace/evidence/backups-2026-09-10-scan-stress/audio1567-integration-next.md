# Next integration work after SIMD

Existing ES8311 kernel worker1158 and atomic DMA-error preparation already exist in source; do not reimplement them or remove Kconfig guards blindly. See full audio-demo-integration-audit.md including follow-ups.

Current outstanding integration is asynchronous APB/user-heap lifetime and upper status memory in a kernel/MMU image. ES8311_SHARED_DUPLEX returns buffers to the upper callback BEFORE dropping its final driver reference, so assuming the user owner always retains the last reference is unsafe (client may free immediately on dequeue). A worker-only address-environment switch also does not establish the correct per-task heap for finalfree. A solution must handle both sample and metadata lifetime, callbacks, close/drain, and status mapping.

Current audio_open additionally allocates priv withkmm_zalloc but its error path useskumm_free and misses freeing priv when mutexlockfails. This is a concrete allocator mismatch to fix with the lifecycle changes, not proof of the mainMMU integration.

PhysicalRESET observer1568 is prepared for current1565 receipt and never toggles resetlines; not launched pending user readiness. Coldpower10 additionally needs uninterrupted earlyboot capture and actualpoweraction; softwareRESET cannot replace it.
