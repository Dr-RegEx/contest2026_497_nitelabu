# 864 final clock and monitoring assessment — evidence988

Final sample: September16 16:43:17 +08. The recovery collector exited normally
and released UART; the host-only monitor observed this terminal state at16:43:20.
No reset, flashing or provisioning was performed during monitoring.

Host realtime elapsed lower bound:86,405.06939482689 seconds. Board date elapsed
interval:86,403..86,405 seconds including one-second display quantization.
Final board-minus-PC error interval:-1.6835227012634277..-0.6628818511962891
seconds. Actual duration exceeds24h and the final absolute error is below2s.

Four scheduled date checks are recorded, plus the original baseline and one
08:14 recovery diagnostic. The diagnostic is not counted as a scheduled check.
The first two scheduled checks occurred at elapsed21,071.261 and41,401.473
seconds, earlier than the prescribed21,600 and43,200 seconds. Later checks
occurred at64,800.116 and86,405.069 seconds. Preserve these actual timestamps.
The original host logger stopped at06:33:23; recovery began around08:12 after
a WSL restart. The user reported uninterrupted board power. Recovered state
supports continuation, but is not proof of an uninterrupted captured log.

Disposition: TIME_METRIC_SATISFIED_WITH_PROCEDURE_DEVIATIONS. The strict original
1.3.14 case is NOT counted PASS, because the every6h procedure was not fully met.
Do not confuse this procedural limitation with a failed final clock metric.
Common confirmed count remains26/35; the separately archived12h standby PASS
remains unchanged. No automatic restart of the24h run was performed.

The requested host-only10-minute monitor produced25 regular observations from
12:38 through16:38 and one terminal observation at16:43:20. All regular
observations found the collector live, state RECOVERY_RUNNING and a fresh log.
These host checks do not send additional date commands or change xTS cadence.
The recovery UART log contains508 complete resource rows, no scanned fault
markers, minimum free522,728 bytes and maximum used5,652 bytes.

Raw source files are preserved unchanged. The original longrun JSON retains
its stale RUNNING field; final status is in the recovery JSON and this derived
assessment. audit-longrun-evidence.py intentionally keeps original-log-only
metrics distinct from recovered_time_observation. Do not interpret stale
original-only duration fields as the final combined duration.

Evidence includes the exact local original xTS text, raw logs/state, collector
source, read-only auditor,10-minute observations and final derived assessment.
SHA256SUMS covers the frozen files. Full-board firmware acceptance remains
separate and pending.
