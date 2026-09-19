# TCP-load timebase review (pending measurement)

1625 host receiver completed 74.7 MiB in 380.05 seconds. Target command requested 300 seconds; target final serial report was not captured. Do not infer exact clock drift from this incomplete run alone. NSH responded and ps showed no iperf tasks afterward.

1626 repeats the original 300-second command with target-file output, an uninterrupted serial owner, and 600-second host observation. It captures /proc/uptime before and after, plus independently timed Windows receiver totals. No target reset between these runs.

Current kernel1623 uses CONFIG_USEC_PER_TICK=1000, no SCHED_TICKLESS or RTC_HIRES. Its common/espressif/esp_timerisr.c clears the periodic alarm and calls nxsched_process_timer once per delivered interrupt. It does not account for multiple elapsed hardware periods while interrupts are delayed.

Read-only reference: s31-reference/tmp/esp-idf-clean/components/freertos/port_systick.c, SysTickIsrHandler, explicitly calculates elapsed hardware periods minus handled ticks and processes missed ticks. This is a source-level hypothesis for the loaded S31 port, not proof that this is the only delay.

Await1626 full report and timing comparison before changing the timer. Any candidate must preserve CPU0 as sole owner of global ticks, keep non-S31 behavior unchanged, retain missed-tick debt if work is bounded, and verify both idle time and loaded time plus task completion on board. Do not change the original xTS300-second command or edit prior long-test results.
