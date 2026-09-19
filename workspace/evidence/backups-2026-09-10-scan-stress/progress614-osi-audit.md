# Linked Wi-Fi OS ABI audit614–616, 2026-09-14

Previous goal turn progressed: f65fe96c6d2 fixes interrupt masks, build609,
network611 DNS/TCP/UDP3/3 and demo612/HTTP613 PASS, sealed613.
Current HEAD remains f65fe96c6d2, Apps ae0dfd89c. No source/config/build/flash
changes this group; board keeps609 and resident demo612. Six WIP edits intact.

New diagnostic audit-wifi-osi614.py preprocesses with each successful build's
actual compiler command, drops object/dependency-output flags, and pipes
preprocessing output. NuttX forces the out-of-tree config.h first so unrelated
source-tree minimal Make config cannot override it. Parses actual preprocessed
wifi_osi_funcs_t declarations and each ELF32 symbol/section table using Python
stdlib, reads g_wifi_osi_funcs bytes, matches pointer targets to function symbols.
Checks ELF32 little endian/RISC-V, fieldcount/size, version9/magicdeadbeaf,
identical ABI field order and all nonzero callback addresses resolve to funcs.
It does NOT assert that callback implementations have correct OS semantics.

Commands from workspace:
python3 D/audit-wifi-osi614.py > D/logs/host614-wifi-osi.log 2>&1
python3 D/audit-wifi-osi614.py > D/logs/host616-wifi-osi-checked.log 2>&1
Both exit0; second adds explicit nonzero-function-target assertions. D=thisdir.
Full preprocess commands and ELF SHA256s in logs. Inputs currentN609 and
diagnostics/idf-baseline/build-hal-wifi/s31_wifi_baseline.elf (priorIDF173).
No reread/flash/rebuild of IDF. CurrentN is b/g/n, not the HE601 image; this
audit does not imply an independent readback of board RAM or HE601 table.

Findings:
- Both tables128fields/512bytes, identicalorder, correctversion/magic.
- Both have NULL _coex_condition_set at0x194; not NuttX-only omission.
- NuttX additionally NULL at0x1d0 regdma_link_set_write_wait_content and0x1d4
  sleep_retention_find_link_by_id. IDF has real function pointers. Current
  adapter supplies these only under CONFIG_MAC_BB_PD, disabled here. Four
  retention attach/detach slots are present; IDF source likewise returns1
  without MAC_BB_PD. objdump proves IDF173 BBattach is li a0,1;ret, so a
  successful IDF HE baseline does not require real BB-retention attach.
  This does not prove the two NULL slots are unreachable in every mode.
- Five NuttX event_group callbacks point to DEBUGPANIC stubs (create/delete/
  set/clear/wait). IDF uses actual FreeRTOS eventgroups. Current DEBUG_ASSERTIONS
  is enabled; objdump confirms create calls __assert. No such assertion was
  observed in HE603. Therefore definite completeness debt, not proven HE cause.
- Peripherals clock enable/reset hooks and slowclk calibration are implemented;
  S31 coex critical wrappers use recursive per-lock spinlockirqsave/restore.
  No basis to replace these with guessed stubs or blanket IDF source copies.

Actual-code review rejects naive nxevent forwarding for FreeRTOS eventgroups:
NuttX sched/event/event_wait.c returns matched subset, zero on timeout, and
touches event->lock after wake. FreeRTOS event_groups.c snapshots whole event
bits and can return partial bits on timeout. nxevent_destroy posts all bits
but does not wait for waiters to stop touching object; kmm_free immediately
after it would risk UAF. Need explicit snapshot/waiter-lifetime design and
multithread timeout/set/delete tests before implementing missing callbacks.
Relevant files inspected: include/nuttx/event.h, event_wait/post/destroy/clear.c,
IDF esp32s31 esp_adapter.c and FreeRTOS-Kernel/event_groups.c.
Library nm search found no named eventgroup wrappers; indirect table calls
and ROM paths mean absence of symbol names does NOT prove unused callbacks.

Initial discovery misses: common/espressif/esp_coex_adapter.c, S31
hal_backports/include/sdkconfig.h, common/espressif/esp_freertos.* do not exist.
Used rg discovery to locate esp32c6/esp_coex_adapter.c and S31/include/sdkconfig.h.
Rvenv elftools import failed ModuleNotFoundError; no dependency downloaded,
used standalone ELF32 reader instead. These are tooling/discovery misses,
not target compile errors (no target compilation in this group).

Objdump commands (T=existing riscv32-esp-elf-objdump):
T -d --disassemble=event_group_create_wrapper PD/nuttx
  logs/host615-event-group-code.log
T -d --disassemble=esp_phy_wifi_bb_sleep_retention_attach_wrapper
  diagnostics/idf-baseline/build-hal-wifi/s31_wifi_baseline.elf
  logs/host615-idf-retention-code.log
Both exit0. PD=openvela-dev/out/esp32s31-cmake-demo.

HTTP616 uses existing Windows Python test_http.py 192.168.1.60 --samples3,
no board reset. Exit0, three samples PASS; existing demo remains available.
No new code commit. checkpoint616.sh seals diagnostic artifact and audit
evidence with prior613 checksum link. No active process; PD/board still609.
Next concrete work: implement missing eventgroup semantics with explicit
waiter ownership/snapshot tests, while keeping HE root-cause status unproven.
