# Required scope addition: ESP32-S31 PIE/SIMD

User explicitly requires SIMD adaptation, additional to the88-case sprint set. Do not silently fold this into an already-passed xTS heading.

Verified against locked local ESP-IDF6.1:
- soc/esp32s31/include/soc/soc_caps.h: SOC_CPU_HAS_PIE=1, preferred alignment16.
- components/riscv/vectors.S:266: S31 PIE available only on CPU1; CPU0 faults receive special handling.
- FreeRTOS portable/riscv/portasm.S:237..380: eight128-bit Q registers, QACC halves, UA_STATE, XACC and SAR/FFT fields; aligned state save required.
- CSR_PIE_STATE_REG=0x7f2. Existing supervisor-mode port needs an appropriate privilege/monitor strategy, not direct assumptions that an M-mode CSR is accessible in S-mode.

Toolchain probe: existing esp15.2.0_20251204 compiler rejects PIE assembly with base rv32imafc_zicsr_zifencei; succeeds with added_xespv. Objdump confirms actual esp.vld.128.ip, esp.vadd.s32, esp.vst.128.ip encodings. No download, no scalar substitution. Object not executed on board.

Implementation acceptance: CPU1 execution, real vector arithmetic compared with scalar reference, full PIE state preserved across preemption/blocking and multiple tasks, coexistence with SMP/Sv32 and existing FPU, explicit CPU affinity/migration policy and unsupported-CPU behavior. AI model/framework acceleration is separate from instruction support; do not claim it merely from this probe.

Current chip riscv_extctx.S already saves CLIC state through INTXCPT_EXTREGS=1. Extend using verified alignment/frame sizing and task initialization; cover interrupt, task-context and signal restoration paths. Current1457 test remains active, no reset/flash for SIMD.

## Context helper implementation

esp32s31_pie.S now contains complete leaf save/restore routines, assembled successfully with the existing base march by using local .option arch,+xespv. They are not yet in the firmware build list, not called and not target-tested. The movx register operand encoding rejects t0/t1; use allowed a1/a2 as in the locked vendor reference. Raw state occupies216 bytes; caller buffer224 bytes, alignment16.

Integration constraint found: existing exception frames are not uniformly16-byte aligned. Do not dynamically align an in-frame pointer and later memcpy the frame to a differently aligned address: that changes the state offset. Use fixed offsets with an aligned bounce area (or explicitly establish aligned frame ABI at every allocation/copy site). Cover up_saveusercontext, supervisor syscall save_extctx_task, trap save/restore, and signal frame copies. Do not claim scheduler support before integration and two-task preemption verification.
