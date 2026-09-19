/* SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * PIE bank layout follows ESP-IDF portable/riscv/portasm.S.
 * Buffers are 16-byte aligned. Scratch registers: a2, a3.
 */
#ifndef __ESP32S31_PIE_MACROS_S
#define __ESP32S31_PIE_MACROS_S

.macro s31_pie_bank_save base
  .option push
  .option arch, +xespv
  esp.vst.128.ip q0, \base, 16
  esp.vst.128.ip q1, \base, 16
  esp.vst.128.ip q2, \base, 16
  esp.vst.128.ip q3, \base, 16
  esp.vst.128.ip q4, \base, 16
  esp.vst.128.ip q5, \base, 16
  esp.vst.128.ip q6, \base, 16
  esp.vst.128.ip q7, \base, 16
  esp.st.qacc.l.l.128.ip \base, 16
  esp.st.qacc.l.h.128.ip \base, 16
  esp.st.qacc.h.l.128.ip \base, 16
  esp.st.qacc.h.h.128.ip \base, 16
  esp.st.ua.state.ip \base, 16
  esp.st.u.xacc.ip \base, 0
  esp.movx.r.sar.bytes a2
  slli a3, a2, 4
  esp.movx.r.fft.bit.width a2
  or a3, a3, a2
  sb a3, 5(\base)
  esp.movx.r.sar a2
  sb a2, 6(\base)
  .option pop
.endm

.macro s31_pie_bank_restore base
  .option push
  .option arch, +xespv
  esp.vld.128.ip q0, \base, 16
  esp.vld.128.ip q1, \base, 16
  esp.vld.128.ip q2, \base, 16
  esp.vld.128.ip q3, \base, 16
  esp.vld.128.ip q4, \base, 16
  esp.vld.128.ip q5, \base, 16
  esp.vld.128.ip q6, \base, 16
  esp.vld.128.ip q7, \base, 16
  esp.ld.qacc.l.l.128.ip \base, 16
  esp.ld.qacc.l.h.128.ip \base, 16
  esp.ld.qacc.h.l.128.ip \base, 16
  esp.ld.qacc.h.h.128.ip \base, 16
  esp.ld.ua.state.ip \base, 16
  esp.ld.xacc.ip \base, 0
  lbu a2, 5(\base)
  esp.movx.w.fft.bit.width a2
  srli a2, a2, 4
  esp.movx.w.sar.bytes a2
  lbu a2, 6(\base)
  esp.movx.w.sar a2
  .option pop
.endm
#endif
