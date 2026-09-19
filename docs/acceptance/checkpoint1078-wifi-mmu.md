# Wi-Fi 1078: capture the missing MMU fault state

BUILD PASS / TARGET NOT RUN / ROOT CAUSE NOT YET CONFIRMED. This is a diagnostic candidate, not a claimed scan fix.

Frozen 967 completed 48 scans, then faulted on CPU1 during PHY temperature tracking: EPC 0x4003c5c2, load page fault (13), TVAL 0x20818000. The address is exactly the locked S31 TSENS control register. The failing path is `temperature_sensor_hal_get_raw_value` -> `temp_sensor_get_raw_value` -> `phy_param_track_tot` -> PLL tracking timer callback -> hr_timer. It must remain enabled.

Bounded source/ELF findings:

- The actual 967 `esp32s31_mm_init` disassembly, not only current source, creates supervisor-only 4 MiB identity mappings across the physical space except the dedicated user window. 0x20818000 belongs to index 130 and should have root PTE 0x082000ef. Missing static mapping is not supported by this evidence.
- Leaf construction sets valid/read/write/execute/global/accessed/dirty flags. User address spaces copy the kernel root entries, and CPU1 installs the kernel root before scheduling.
- Each hart's M-mode setup grants the physical map through PMP entry 0; finer user restrictions are in Sv32. The hart-specific PMA preparation adjusts Flash/PSRAM, not TSENS. No evidence currently justifies disabling SMP, calibration, or modifying peripheral mappings.
- Existing panic logging prints cause/EPC/TVAL, then dumps the panic path registers. It omits the active SATP, original saved privilege/status, and faulting PTE; therefore it cannot distinguish invalid/corrupt active tables, privilege-state errors, or hardware access classification.

1078 adds fault-only S31 kernel diagnostics: live SATP/status, saved status, kernel root/PTE, active root/PTE and bounded L2 PTE if applicable. Root reads are restricted to the internal SRAM range reserved for page tables to avoid dereferencing arbitrary corrupt SATP addresses. The original fatal fault handling is unchanged. No mapping, calibration, affinity, or timeout changes were made.

Source: `esp32s31_mm_init.c` and a chip/kernel-scoped page-fault hook in `common/riscv_exception.c`. The candidate also inherits already-integrated project changes since 967, including the corrected factory two-address MAC configuration. A future successful run cannot by itself establish the old page-fault mechanism; retain diagnostic/provenance distinction.

Independent output: `openvela-dev/out/esp32s31-wifi1078`.
Kernel size: 1174060 bytes; paired AppFS: 2348032 bytes.
Receipt: `build1078-wifi-mmu.sha256`.
Helpers: `build1078-wifi-mmu.sh`, `flash1078-wifi-mmu.sh`.
Build log: `logs/build1078-wifi-mmu.log`.

Build exited zero. Kernel/AppFS receipts verified, as did frozen 967. Existing Wi-Fi artifact checker passed (`booleans=1754 protocol=7`); SMP/MMU kernel profile and radio configuration retained. No UART operation was performed. Build resource released. Root should run the original fresh scan100 and retain any `S31 MMU:` lines before the panic.
