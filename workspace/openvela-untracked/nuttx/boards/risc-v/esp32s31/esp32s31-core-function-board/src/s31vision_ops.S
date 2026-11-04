/* SPDX-License-Identifier: Apache-2.0 */
/* CPU1 PIE only: signed INT8 dot, aligned inputs, length divisible by 16. */
.text
.option push
.option arch, +xespv
.balign 4
.global s31digit_dot
.type s31digit_dot, @function
s31digit_dot:
  addi sp, sp, -16
  esp.zero.xacc
  li a3, 0
  esp.movx.w.sar a3
1:
  esp.vld.128.ip q0, a0, 16
  esp.vld.128.ip q1, a1, 16
  esp.vmulas.s8.xacc q0, q1
  addi a2, a2, -16
  bnez a2, 1b
  mv a3, sp
  esp.st.s.xacc.ip a3, 0
  lw a0, 0(sp)
  addi sp, sp, 16
  ret
.size s31digit_dot, .-s31digit_dot
.option pop
