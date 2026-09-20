# PHY timer MMU fault

UDP TX1386 start: CPU1 hr_timer fault at temperature_sensor_hal_get_raw_value (EPC4003fc20) loading TSENS20818000. Kernel root PTE082000ef exists; active root2f04d000 PTE index130 is zero. Original300s not run. Host capture stopped only after panic dump.

Source defect: addrenv_switch sets g_addrenv=NULL for kernel task without up_addrenv_deselect. addrenv_uninstall_all searches g_addrenv and therefore cannot locate stale hardware root on that CPU before free.1387 adds deselect before clearing bookkeeping under existing lock. No change to SMP/MMU/privilege. Hypothesis consistent with exact fault; runtime verification pending.1378 scan/DHCP stalls cannot yet be assigned this cause.
