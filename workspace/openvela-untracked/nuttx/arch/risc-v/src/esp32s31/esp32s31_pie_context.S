/* SPDX-License-Identifier: Apache-2.0 */
#include "esp32s31_pie_macros.S"

/* Fixed frame offset survives signal/context copies regardless of alignment.
 * Transfer through an aligned stack buffer: SIMD memory operations cannot
 * safely target the arbitrary word-aligned context pointer directly.
 * All argument and s-registers are preserved; t-registers are scratch.
 */
#define S31_PIE_FRAME_OFFSET (INT_REG_SIZE * (REG_INT_CTX_NDX + 2))
#define S31_PIE_WORDS 54

.macro s31_pie_context_save frame
  addi t3, \frame, S31_PIE_FRAME_OFFSET
  csrr t0, CSR_SCRATCH
  beqz t0, .Lpie_save_zero\@
  lw t0, 4(t0)  /* riscv_percpu_s.hartid */
  li t1, 1
  bne t0, t1, .Lpie_save_zero\@
  mv t5, sp
  addi sp, sp, -240
  andi sp, sp, -16
  sw a2, 224(sp)
  sw a3, 228(sp)
  sw a4, 232(sp)
  mv a4, sp
  s31_pie_bank_save a4
  lw a2, 224(sp)
  lw a3, 228(sp)
  lw a4, 232(sp)
  mv t4, sp
  li t6, S31_PIE_WORDS
.Lpie_save_copy\@:
  lw t0, 0(t4)
  sw t0, 0(t3)
  addi t4, t4, 4
  addi t3, t3, 4
  addi t6, t6, -1
  bnez t6, .Lpie_save_copy\@
  mv sp, t5
  j .Lpie_save_done\@
.Lpie_save_zero\@:
  /* PIE callers must be pinned to CPU1. CPU0 has no PIE register bank. */
  li t6, S31_PIE_WORDS
.Lpie_zero_loop\@:
  sw zero, 0(t3)
  addi t3, t3, 4
  addi t6, t6, -1
  bnez t6, .Lpie_zero_loop\@
.Lpie_save_done\@:
.endm

.macro s31_pie_context_restore frame
  addi t3, \frame, S31_PIE_FRAME_OFFSET
  csrr t0, CSR_SCRATCH
  beqz t0, .Lpie_restore_done\@
  lw t0, 4(t0)
  li t1, 1
  bne t0, t1, .Lpie_restore_done\@
  mv t5, sp
  addi sp, sp, -240
  andi sp, sp, -16
  mv t4, sp
  li t6, S31_PIE_WORDS
.Lpie_restore_copy\@:
  lw t0, 0(t3)
  sw t0, 0(t4)
  addi t3, t3, 4
  addi t4, t4, 4
  addi t6, t6, -1
  bnez t6, .Lpie_restore_copy\@
  sw a2, 224(sp)
  sw a3, 228(sp)
  sw a4, 232(sp)
  mv a4, sp
  s31_pie_bank_restore a4
  lw a2, 224(sp)
  lw a3, 228(sp)
  lw a4, 232(sp)
  mv sp, t5
.Lpie_restore_done\@:
.endm
