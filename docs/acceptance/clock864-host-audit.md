# Longrun 864 clock-source audit (2026-09-15)

The existing board run remains uninterrupted. Do not reset, reflash, or open a
second serial owner while PID 144782 is running.

Sample 1 reports elapsed monotonic time 21600.430604 seconds.
Between samples 0 and 1, host CLOCK_REALTIME advanced 21071.278756 seconds while
recorded CLOCK_MONOTONIC advanced 21599.853614 seconds (difference
528.574858 seconds). The date-comparison interval at sample 1 is
[-1.873315, -0.854678] seconds. This is an intermediate comparison, not a final
24-hour PASS.

Read-only follow-up host measurements are in clock864-host-audit.jsonl.
The cause of the clock-source discrepancy is not established. The running
script schedules against CLOCK_MONOTONIC and compares board date against
CLOCK_REALTIME. Retain both axes and review the final elapsed wall time before
accepting its automatic 12h/24h labels; do not silently treat the previously
estimated calendar deadlines as measured runtime or edit raw results.

Final acceptance remains pending this timing review and completion of the run.

September16: audit-longrun-evidence.py reads the existing JSON/UART only, reports duration bounds on both host REALTIME and board date axes, resource extrema and faults. It never opens UART, edits raw evidence, or automatically adopts a PASS label. At00:38 it found476 complete resource rows, unchanged free522728/used5652 and no post-monitor fault markers. Only samples0/1 existed. If sample4 occurs before24 actual hours, retain its output and continue the same unrebooted board observation before a later date comparison; do not flash merely because the original process exits.

00:54 read-only source review: on-target860 has RTC_DRIVER=y, HR_TIMER=y,
RTC_HIRES=n, SCHED_TICKLESS=n, USEC_PER_TICK=10000. nxclock_gettime REALTIME
adds g_basetime to clock_systime_timespec, which uses g_system_ticks in this
configuration. esp_timerisr.c configures a hardware periodic SYSTIMER alarm
and increments one OS tick per handled interrupt; it does not rearm relative
to handler completion. This rules out a simple relative-rearm latency error.
Long masked intervals could coalesce ticks, but no such loss has been measured
in864; crystal/host clock differences are also unresolved. Do not change the
running image or preemptively claim a proven clock-driver defect.

The auditor now also reports host elapsed time to the latest raw-log write
and complete showinfo periods for ongoing standby. Both must reach12h, logs
must stay live and fault-free, and the timing review must be resolved before
accepting standby. A later6h date sample is not required just to recognize a
completed12h continuous monitor interval. At00:54:491complete rows,29400s of
complete monitor periods and29405.687s host observation; still below12h.

01:58 host availability check: Windows reports ACLineStatus1 and no battery
(BatteryFlag128). AC automatic sleep and hibernate timeout are both0 (never);
DC sleep is600s. Read-only powercfg/GetSystemPowerStatus evidence saved in
clock864-host-power-query.json. No power policy or clock source was changed.

Same-boot extension preparation: extend-longrun-clock.py is syntax checked,
NOT EXECUTED. It refuses an incomplete/error original run or occupied UART.
If original evidence already covers24h on both axes, it does not open UART.
Otherwise it opens the existing raw115200 tty without termios or DTR/RTS
updates, waits for an existing showinfo row, and confirms the original monitor
task survives. OS-level line behavior on open is not assumed harmless: any
boot/fault marker or absent original monitor rejects continuity without retry.
Only ps/date commands are sent. No reset, time-setting, radio or Flash action.
It preserves original JSON/UART hashes and writes fresh extension evidence.
It stops at the first comparison meeting duration, regardless of whether the
time error passes, and returns COMPLETE_REVIEW_REQUIRED rather than PASS.
Choose a fresh numbered tag when actually needed after the original exits.

Follow-up source-only contingency: CONFIG_RTC_HIRES changes
clock_systime_timespec from g_system_ticks conversion to up_rtc_gettime minus
g_basetime. With RTC_DRIVER and g_hr_timer_enabled, Espressif up_rtc_gettime
reads esp_hr_timer_time_us (SYSTIMER_COUNTER_ESPTIMER) plus the RTC offset/base.
This is a free-running counter path rather than counting handled tick IRQs.
It is a possible follow-up only if the final result demonstrates a clock
problem; no RTC_HIRES config or clock source was changed or built. Its boot,
set-time, alarm/periodic and scheduling behavior would require hardware
verification before adopting it. Current discrepancy cause remains unknown.

03:11 independent read-only review confirmed the HIRES read path has no direct
RTC/HR recursive lock and the early RTC initialization precedes HR bring-up.
It also identified a prerequisite for any HIRES fallback: esp_rtc_driverinit
currently reinitializes the RTC lock, publishes g_hr_timer_enabled, then writes
the 64-bit offset without holding that lock (esp_rtc.c:955). Under SMP a reader
could observe the intermediate state. A fallback must preserve the existing
lock and commit offset/enabled together under it. This is a source-level race,
not evidence that it caused864's observed discrepancy. No patch or alternate
image has been applied. Also review the basetime/RTC transition during settime;
HIRES does not by itself replace periodic scheduler ticks or change the
reported POSIX clock resolution. Retain the original run until its measured
duration and date results can be assessed.

03:50:54 Windows read-only w32tm /stripchart single query to
time.windows.com(52.231.114.183:123) completed with reported offset
-1.4359775s. Structured observation:clock864-external-time-query.json. No
resync or clock-setting command ran. One external sample does not establish
full-run clock stability or explain the MONOTONIC/REALTIME duration mismatch.
The published xTS comparison remains board versus PC, not versus this server;
do not use the external offset to waive a failing board/PC error.

04:13 original sample2 arrived: boardUTC20:13:14, PC error interval
[-0.085811,+0.933120]s. Host REALTIME since sample0 is41401.47..41401.51s;
MONOTONIC is43199.73s, differing by1798.23s. The script automatically labels
standby PASS, but actual continuous observation is only about11h30m. Do not
adopt this early label. Keep the same boot/showinfo run until both actual
monitor periods and host REALTIME cover43200s. No evidence currently supports
a board clock-driver failure. Snapshot:clock864-evidence-audit-sample2.json.

04:46 standby accepted only after actual duration: frozen723 resource
records,43329.712887s host REALTIME and43320s complete monitor periods; no
faults/reset and free522728 bytes unchanged. checkpoint864-standby12h contains
raw JSON/UART/config/receipt, audited result and hashes; evidence archive
verified. Original864 is still running. Common3.1.1 PASS does not imply
1.3.14 time consistency PASS; the latter remains pending full24h review.

## September16 recovery and acceptance limits

WSL restarted at08:05; original logger ended06:33. Reattached sameCP2102
serial and observed originalshowinfo PID7, matching resource values and board
date within1s. User reports board continuously powered. Recovery loggerPID6011
is active from08:14; original logs/JSON remain unchanged. No reset/time set.
Original1.3.14 asks date comparisons every6h and<=2s after>=24h, not explicitly
continuous serial logging. Missing06:33..08:12 resource logs and the earlier
MONOTONIC/REALTIME scheduling deviation remain disclosed acceptance risks.
Do not promise acceptance or markPASS before actual24h review.12h standby
archive predates the gap and remains valid. See xts864-acceptance-deviations.json.

User clarification: follow original6h/four-check requirement only. Cancelled
hourly recovery sampling before any hourly check occurred. Recovery08:14
sample is diagnostic, not an added acceptance requirement. Remaining checks
are actual18h(10:43) and>=24h(16:43). Host logger was restarted to change its
schedule without board reset or date setting. Original records retained.
