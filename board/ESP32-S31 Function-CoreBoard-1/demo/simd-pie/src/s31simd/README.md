# ESP32-S31 CPU1 SIMD validation

Enable `CONFIG_ESP32S31_PIE` and `CONFIG_EXAMPLES_S31SIMD=m`; run `s31simd` from NSH. The `demo-rmt-bttool-coex-pie` profile includes this command.

Two workers start on CPU0, alternate their affinity 20 times while checking the actual CPU after each change, then remain on CPU1 before executing any PIE instruction. Each runs 100 rounds of actual four-lane unsigned vector addition checked against scalar results, followed by a full-bank load/sleep/store check (Q0–Q7, QACC L/H, UA_STATE, XACC, SAR_BYTES, FFT_BIT_WIDTH and SAR) with distinct per-worker/per-round patterns. A sleep invokes the normal user/kernel syscall and scheduling path. Every thread must complete 100 rounds without mismatches. CPU0 is not PIE-capable.

This is a development test, not an original xTS case. It does not support executing PIE on CPU0 or provide an AI inference framework. The optional context support must be enabled for applications that use these instructions; users must pin SIMD threads to CPU1. Applications must explicitly bind to CPU1; implicit migration on illegal instructions is not provided.

Instruction forms and register-bank layout were checked against the project's locked ESP-IDF reference, `components/freertos/test_apps/freertos/port/test_pie_routines.S` and `components/freertos/FreeRTOS-Kernel/portable/riscv/portasm.S`. No SDK or repository download is required.

Target evidence: checkpoint1560-simd-fullbank, paired build1559. Both workers completed100 rounds with zero errors. Earlier1556 runtime self-affinity attempt executed onCPU0 and faulted. The shared scheduler ready-list fix in1562 was verified by1563: both workers completed20 cross-core affinity changes then100 SIMD rounds.

INT8 compute demo: `s31simd_dot16` executes sixteen signed 8-bit multiply-accumulates using `esp.vmulas.s8.xacc`. The operands are16-byte aligned, each16-element sum fits int32_t, and each worker checks100 signed datasets against scalar arithmetic. Target1566 on1565 pair passed200 datasets plus migration and full-bank checks. This demonstrates a building block used in quantized inference, not an inference framework or measured speedup.
