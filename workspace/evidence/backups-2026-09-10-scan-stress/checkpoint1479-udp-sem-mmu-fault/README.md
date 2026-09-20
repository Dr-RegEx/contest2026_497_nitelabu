# 1479 UDP diagnostic MMU fault

CPU0 Store/AMO page fault EPC4002a3fa MTVAL61401394 SATP8002f015 (kernel root).1469 ELF resolves nxsem_wait_irq atomic increment after addrenv_restore. sem_waitirq.c selects waiting task user address environment, removes waiter, restores old kernel environment, then restore_context writes user sem count. Kernel-only page table has no user semaphore mapping. Fix must finish semaphore mutations before restoring environment, but restore environment before making task runnable/context switch. Original1479 remains FAIL; preserve before reset.
