# SIMD target acceptance scope

The CPU1-only PIE extension remains a mandatory adaptation. Existing xTS70/88 is separate; do not increase it for these development checks.

1.1554 proves the extended SMP/MMU image boots, not arithmetic.
2.1556 failed at first Q load on CPU0 despite a runtime affinity request. Keep this failure and investigate migration separately; never execute PIE until CPU1 ownership is established.
3.1557 creates workers with CPU1 affinity before activation and verifies the actual CPU.1558 will check actual four-lane arithmetic and two workers retaining distinct Q0-Q7 values across100 sleep/syscall transitions each.
4.Full-bank context claim additionally requires auxiliary QACC/UA_STATE/XACC/SAR state evidence; the current Q test alone cannot establish it.
5.Final integration must retain the competition/network/Bluetooth functionality and demonstrate a usable compute workload. No implicit claim of an AI inference SDK or automatic CPU0 migration.
