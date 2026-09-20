# User priority correction

2026-09-17: prioritize the prior88-case sprint test set; SIMD must come after Bluetooth. SIMD remains required but is deferred.

Active1457 original100 Wi-Fi reconfiguration remains running on1455. Next1458 RSSI;1461 TCP close diagnostic is built and not flashed. ExistingBLE1085 original enable/state/disable/state PASS remains valid for its isolated profile; do not claim fullBLE/GATT or coexistence support, and do not repeat passed enable/disable solely to reorder work. BroaderBluetooth gaps and remaining selected test gaps precedeSIMD.

SIMD1466 build failed in riscv_exception_common.S (assembler1-byte field overflow value402); NOTFLASHED, NOTPASS. Preserve prepared code and logs. Competition defconfig restored exactly to competition-defconfig-before1466, disabling optionalPIE integration; no active SIMD build. No changes to running1455.
