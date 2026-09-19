# Audio integration into the SMP/MMU demo: bounded source audit

Read-only product-source audit; no build, UART, provisioning, HAL or IDF change.
Baseline: frozen 1057 continuous 44.1 kHz audio and 1078 SMP/Sv32 demo.
1131/1143 native 16 kHz capture and listening evidence remains separate.

## Conclusion

This is not a Kconfig-only integration. Internal DMA buffers already suit the
identity-mapped kernel, but the asynchronous audio API has user-address-space
and codec worker-mode gaps. Removing BUILD_FLAT/!SMP/!WIFI guards alone is unsafe.
A bounded fixed-44.1-kHz integration is possible in principle, but requires real
kernel audio lifecycle work plus a new integrated target run. It cannot inherit
1057's runtime PASS. Do not disable SMP/MMU to claim an integrated demo.

## Findings and smallest necessary work

1. **DMA addresses: no demonstrated need for a new allocator.**
   `arch/risc-v/src/esp32s31/esp32s31_i2s_duplex.c` keeps 4096 bytes of payload
   plus aligned descriptors in DRAM_ATTR internal SRAM. Only these buffers,
   never application APBs, are submitted to DMA. `esp32s31_mm_init.c:201`
   identity-maps kernel physical ranges, including internal SRAM and peripheral
   MMIO. Preserve those properties and the uncached/internal runtime checks;
   verify actual integrated map addresses and memory headroom. Do not translate
   arbitrary APB virtual pointers into DMA addresses. Existing descriptor
   ownership barriers remain necessary on both CPUs.

2. **Actual MMU blocker: APB lifetime and address environment.**
   `libs/libc/audio/lib_buffer.c:61` allocates both APB metadata and sample
   memory with user allocators. `audio/audio.c:840` forwards the application
   buffer directly. `dup_enqueue` retains it; `dup_service`, completion and
   callbacks dereference it from a boot-created `s31_duplex` kernel thread.
   `sched/task/task_init.c:235` explicitly gives kernel threads no owned user
   address environment. The driver neither retains nor selects the submitting
   process environment. Pinning this worker to a CPU does not solve this.
   Smallest credible kernel integration must define ownership at the audio
   upper-half boundary: retain the originating environment for the complete
   asynchronous lifecycle, switch only in sleepable context for user-buffer
   access/callback bookkeeping, and release on drain/STOP/error/close; or use
   a kernel-buffer/shared-mapping API with explicit copy-in/out. Do not patch
   only the DMA memcpy: codec pending/done lists and APB reference/free paths
   also touch user metadata. Process exit while buffers are queued must have
   bounded cancellation. Existing flat behavior should remain unchanged.

3. **Actual worker privilege blocker.**
   `drivers/audio/es8311.c:1799` creates its codec worker with pthread_create
   and later joins it. `sched/pthread/pthread_create.c:153` invokes
   up_pthread_start in non-flat builds; `arch/risc-v/src/common/
   riscv_pthread_start.c:68` unconditionally transitions to user mode. A
   kernel-address es8311_workerthread cannot run through that entry path.
   Add a kernel-thread lifecycle for BUILD_KERNEL (including argument delivery,
   startup failure rollback, exit completion and STOP/join semantics). Simply
   replacing pthread_create without matching teardown is insufficient.

4. **SMP locking/IRQ: existing infrastructure is useful, not sufficient proof.**
   Queue operations use g_lock; callback delivery is outside it. Existing
   common/espressif/esp_irq.c:535 routes S31 IRQs to the configuring CPU and
   esp_setup_irq allocates under enter_critical_section. No new IRQ controller
   or HAL implementation is indicated. However ep->errors is ORed by the ISR
   and by dup_service and reset by dup_start_dma; not every access shares one
   synchronization primitive. Make error publication/clear atomic or protect
   all accesses with the same ISR-safe spinlock. Keep initialization serialized
   in board bringup (g_ready itself is not a concurrent initialization guard).
   Review route enable/teardown on the owner CPU; a migrating caller must not
   manipulate a different CPU's CLIC enable. Do not hold locks across callbacks
   or perform address-environment switching in ISR context.

5. **Resources: GPIO/RMT is not the demonstrated blocker; Wi-Fi coexistence
   remains unproven.** Audio reserves I2S0, AHB DMA RX/TX channel 0, GPIO52--56,
   PA57 and I2C0 codec address0x18. Existing RMT implementation uses its local
   item RAM, not this driver's DMA ring. Board I2C initialization can share
   the existing bus; bus transfer locking must remain intact. Do not issue
   concurrent manual writes to the codec while streaming. Before allowing
   ESPRESSIF_WIFI together, verify all enabled DMA owners and global clock/
   DMA-reset operations, including linked binary Wi-Fi components; source-only
   inspection here does not establish radio coexistence. Offline demo commands
   running in a Wi-Fi-capable image are not proof of concurrent audio/radio.
   Audio ISR is not registered IRAM-safe, so do not promise uninterrupted audio
   during Flash erase/cache-off work without separate evidence.

6. **Configuration and memory follow the fixes.** 1078's actual config is
   BUILD_KERNEL, ARCH_ADDRENV, S-mode Sv32, SMP_NCPUS=2, CPUSET0x3, Wi-Fi enabled,
   AUDIO disabled and MM_KERNEL_HEAPSIZE=8192. Add only raw PCM/ES8311/continuous
   44.1 kHz and the needed original capture command, not the full media codecs.
   Recalculate kernel heap and stack headroom: the additional duplex worker
   alone requests 4096 bytes before codec worker and queue objects. Retain the
   frozen baseline and create a distinct candidate/config; relax protection
   only behind a default-off kernel integration option after the above work.

## Executable priority

P0: Keep 1078 demo and 1057/1131 original audio evidence individually reproducible;
state honestly that a single SMP/MMU+audio image is not yet validated.

P1: If a single image is required for submission, first implement the ES8311
kernel worker and a reviewed audio upper-half address-environment/lifetime
contract. These are the gating tasks, before Kconfig or a firmware build.
Fix the narrowly identified error-state synchronization and audit DMA ownership
in parallel. Preserve fixed format and existing original use cases.

P2: Build one new SMP/MMU candidate, verify map/heap/IRQ placement, then run the
existing demo status/RGB/I2C/BOOT flow and original 44.1 kHz recording with user
listening. Recheck orderly STOP/FINAL completion. Add simultaneous Wi-Fi only
when actually required and resource ownership is resolved; do not convert
separate PASS records into a coexistence PASS.

The MMU audio API/lifecycle work is larger than a board configuration tweak.
For the competition, expanded formats, media decoders and generalized DMA
allocation are unnecessary for this integration and should not be added.

## Follow-up source preparation1158 (not integrated target acceptance)

The DMA error field now uses NuttX atomic_or/atomic_xchg for ISR publication, worker updates and read-clear. Standalone RV32 object compilation with1131 generated configuration passed, with no unresolved atomic helper; patch and pre-change source are retained in i2s1158-atomic.patch and i2s-duplex-before1158.c. Initial compile-header mismatch is retained separately.

ES8311 now has a BUILD_KERNEL kthread wrapper and completion-token lifecycle; review artifacts are in es8311-kernel-worker-review. FLAT preprocessing is equivalent and standalone FLAT/kernel/shared/nonshared object compilation passed. Neither change enables the guarded kernel audio configuration. User APB environment/lifetime management, kernel heap and complete integrated runtime verification remain outstanding. Frozen accepted images and xTS counts remain unchanged.

## Root follow-up: address selection alone is insufficient

The1078 config has CONFIG_MM_TASK_HEAP=y. apb_free reaches lib_ufree/kumm_free -> free -> USR_HEAP -> umm_getheap -> task_get_info()->ta_heap (libs/libc/audio/lib_buffer.c, mm/umm_heap/umm_free.c, mm/umm_heap/umm_globals.c, include/nuttx/tls_task.h). addrenv_select changes the current address environment but does not itself replace the kernel worker task TLS heap ownership. An environment-only patch therefore does not prove correct final APB deallocation.

The audio upper half also allocates upper->status in the first opener user heap. audio_dequeuebuffer touches both APB and that status; retaining only request sample pages is insufficient, particularly for multiple openers. In audio_close the open-private entry is removed/freed and lower->shutdown is called while upper->spinlock remains held; its return is ignored and upper->status is freed. Current shared ES8311 shutdown returns-EBUSY if a worker still exists. Thus closing without a completed STOP/RELEASE must be handled before user environment release, and a blocking drain must not be inserted under this spinlock. These findings block merely adding addrenv_select around DMA memcpy or the codec loop. No upper-half/API source changes were made during this audit.
