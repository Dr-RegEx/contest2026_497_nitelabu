# 1480 semaphore timeout address environment fix

1479 precise fault is atomic_add(NXSEM_COUNT(sem),1) after addrenv_restore(oldenv), CPU0 idle kernel root and user sem61401394. Move count/mutex-waiter-bit restoration into nxsem_wait_irq before restoring oldenv. Keep ready-list/scheduler work after oldenv restore. Cancellation/timeout logic and workload unchanged; non-ADDRENV order equivalent. Build and on-board UDP timeout regression pending.
