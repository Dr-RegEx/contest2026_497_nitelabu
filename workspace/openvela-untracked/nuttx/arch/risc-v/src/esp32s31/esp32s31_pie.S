/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_pie.S
 *
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/* ESP32-S31 CPU1 PIE register bank.  Adapted from the register layout in
 * ESP-IDF portable/riscv/portasm.S.  Caller must enable PIE in M-mode,
 * execute on CPU1, and provide a 16-byte-aligned 224-byte kernel buffer.
 * These leaf routines do not change interrupt state or implement scheduling.
 */

  .text
  .option push
  .option arch, +xespv
  .balign 4
  .global esp32s31_pie_save
  .type esp32s31_pie_save, @function
esp32s31_pie_save:
  esp.vst.128.ip q0, a0, 16
  esp.vst.128.ip q1, a0, 16
  esp.vst.128.ip q2, a0, 16
  esp.vst.128.ip q3, a0, 16
  esp.vst.128.ip q4, a0, 16
  esp.vst.128.ip q5, a0, 16
  esp.vst.128.ip q6, a0, 16
  esp.vst.128.ip q7, a0, 16
  esp.st.qacc.l.l.128.ip a0, 16
  esp.st.qacc.l.h.128.ip a0, 16
  esp.st.qacc.h.l.128.ip a0, 16
  esp.st.qacc.h.h.128.ip a0, 16
  esp.st.ua.state.ip a0, 16
  esp.st.u.xacc.ip a0, 0
  esp.movx.r.sar.bytes a1
  slli a2, a1, 4
  esp.movx.r.fft.bit.width a1
  or a2, a2, a1
  sb a2, 5(a0)
  esp.movx.r.sar a1
  sb a1, 6(a0)
  ret
  .size esp32s31_pie_save, .-esp32s31_pie_save

  .balign 4
  .global esp32s31_pie_restore
  .type esp32s31_pie_restore, @function
esp32s31_pie_restore:
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
  lbu a1, 5(a0)
  esp.movx.w.fft.bit.width a1
  srli a1, a1, 4
  esp.movx.w.sar.bytes a1
  lbu a1, 6(a0)
  esp.movx.w.sar a1
  ret
  .size esp32s31_pie_restore, .-esp32s31_pie_restore
  .option pop
