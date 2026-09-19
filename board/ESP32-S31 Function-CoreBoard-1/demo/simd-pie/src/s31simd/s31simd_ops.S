/* SPDX-License-Identifier: Apache-2.0 */
/* CPU1 only. Aligned 128-bit operands. */
.text
.option push
.option arch, +xespv
.balign 4
.global s31simd_add
.type s31simd_add, @function
s31simd_add:
  esp.vld.128.ip q0, a0, 0
  esp.vld.128.ip q1, a1, 0
  esp.vadd.u32 q2, q0, q1
  esp.vst.128.ip q2, a2, 0
  ret
.size s31simd_add, .-s31simd_add

.balign 4
.global s31simd_hold
.type s31simd_hold, @function
s31simd_hold:
  addi sp, sp, -16
  sw ra, 12(sp)
  sw a1, 8(sp)
  esp.vld.128.ip q0, a0, 16
  esp.vld.128.ip q1, a0, 16
  esp.vld.128.ip q2, a0, 16
  esp.vld.128.ip q3, a0, 16
  esp.vld.128.ip q4, a0, 16
  esp.vld.128.ip q5, a0, 16
  esp.vld.128.ip q6, a0, 16
  esp.vld.128.ip q7, a0, 16
  esp.ld.qacc.l.l.128.ip a0, 16
  esp.ld.qacc.l.h.128.ip a0, 16
  esp.ld.qacc.h.l.128.ip a0, 16
  esp.ld.qacc.h.h.128.ip a0, 16
  esp.ld.ua.state.ip a0, 16
  esp.ld.xacc.ip a0, 0
  lbu a2, 5(a0)
  esp.movx.w.fft.bit.width a2
  srli a2, a2, 4
  esp.movx.w.sar.bytes a2
  lbu a2, 6(a0)
  esp.movx.w.sar a2
  li a0, 1000
  call usleep
  lw a1, 8(sp)
  esp.vst.128.ip q0, a1, 16
  esp.vst.128.ip q1, a1, 16
  esp.vst.128.ip q2, a1, 16
  esp.vst.128.ip q3, a1, 16
  esp.vst.128.ip q4, a1, 16
  esp.vst.128.ip q5, a1, 16
  esp.vst.128.ip q6, a1, 16
  esp.vst.128.ip q7, a1, 16
  esp.st.qacc.l.l.128.ip a1, 16
  esp.st.qacc.l.h.128.ip a1, 16
  esp.st.qacc.h.l.128.ip a1, 16
  esp.st.qacc.h.h.128.ip a1, 16
  esp.st.ua.state.ip a1, 16
  esp.st.u.xacc.ip a1, 0
  esp.movx.r.sar.bytes a2
  slli a3, a2, 4
  esp.movx.r.fft.bit.width a2
  or a3, a3, a2
  sb a3, 5(a1)
  esp.movx.r.sar a2
  sb a2, 6(a1)
  lw ra, 12(sp)
  addi sp, sp, 16
  ret
.size s31simd_hold, .-s31simd_hold
/* Sixteen signed INT8 products accumulated in the 40-bit XACC.
 * The bounded sum fits int32_t. All operands must be 16-byte aligned.
 */
.balign 4
.global s31simd_dot16
.type s31simd_dot16, @function
s31simd_dot16:
  addi sp, sp, -16
  esp.vld.128.ip q0, a0, 0
  esp.vld.128.ip q1, a1, 0
  esp.zero.xacc
  li a2, 0
  esp.movx.w.sar a2
  esp.vmulas.s8.xacc q0, q1
  mv a2, sp
  esp.st.s.xacc.ip a2, 0
  lw a0, 0(sp)
  addi sp, sp, 16
  ret
.size s31simd_dot16, .-s31simd_dot16
.option pop
