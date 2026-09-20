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
