# GATT service removal fix

1514 completed real PC GATT reads and the exact 13-byte write. Legacy connectable advertising stopped on connection, so its second explicit stop could not produce a second stop event. The original failure report is retained; 1519 checks the already captured stop event.

1516 cleanup triggered mm_malloc_size.c:78 in gatts stop 3. Source inspection found remove_from_server_db unconditionally freed user_data, while gatt_db_add(..., 0) borrows framework characteristic element pointers. These pointers must not be freed by the SAL.

The fix tracks ownership per attribute, moves ownership flags with compacted attributes, and frees only owned data. Dynamically allocated CCC wrappers explicitly transfer ownership despite using the zero-copy add path. 1518 compiled and flashed but did not run a test; review found the CCC ownership exception before acceptance, so 1520 supersedes it. Both receipts remain frozen.

Acceptance requires the 1519 target run on the 1520 receipt: two exact read values, the complete target-side write byte dump, service stop/unregister, Bluetooth disable and return to NSH. It is not SMP/MMU or Wi-Fi coexistence acceptance.
