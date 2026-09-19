# ESP32-S31 CPU1 SIMD validation

Enable `CONFIG_ESP32S31_PIE` and `CONFIG_EXAMPLES_S31SIMD=m`; run `s31simd` from NSH. The `demo-rmt-bttool-coex-pie` profile includes this command.

Two workers bind themselves to CPU1 before executing any PIE instruction. Each runs 100 rounds of actual four-lane unsigned vector addition checked against scalar results, followed by a Q0–Q7 load/sleep/store check with distinct per-worker/per-round patterns. A sleep invokes the normal user/kernel syscall and scheduling path. Every thread must complete 100 rounds without mismatches. CPU0 is not PIE-capable.

This is a development test, not an original xTS case. It does not yet test non-Q auxiliary registers, arbitrary CPU migration or provide an AI inference framework. The optional context support must be enabled for applications that use these instructions; users must pin SIMD threads to CPU1. No automatic migration is claimed.

Instruction forms and register-bank layout were checked against the project's locked ESP-IDF reference, `components/freertos/test_apps/freertos/port/test_pie_routines.S` and `components/freertos/FreeRTOS-Kernel/portable/riscv/portasm.S`. No SDK or repository download is required.
