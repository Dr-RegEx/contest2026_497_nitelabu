# 1512 GATT command absent

Prior1077 BLE image explicitly disables BLUETOOTH_GATT_SERVER and BT_GATT_DYNAMIC_DB. gatts register3 returns unknown command, no PC GATT transaction started. Advertise1509 remains separately verified. Candidate1513 enables server and dynamic database in existing isolated FLAT profile; no coexistence or SMP claim.
