# Future longrun host scheduler correction983

No new target run. The active resume864-observation.py/PID6841 is unchanged;
its18h sample is already saved as982 and final24h remains around16:43. This
patch does not repair historical cadence deviations or recover missing logs.

The old fresh-run xts-longrun.py scheduled6h nodes and labeled12h/24h using
HOST_MONOTONIC. In this WSL session that axis advanced faster than REALTIME.
The correction schedules four checks at baseline.host_after + index*21600 +5s,
with the initial alignment sample kept separate. The5s margin covers whole-
second date quantization and command I/O; it is not an additional sample or
new test. Short serial/heartbeat watchdogs still use monotonic, recorded for
diagnosis. Before acceptance, both host REALTIME and conservative board-date
span must reach the node. No original xTS duration/count/workload is reduced.

Heartbeat refresh now requires a newly completed resource row, not a stale
row remaining in a rolling buffer. Only CPU0/no-prefix rows count to avoid
counting multiple CPUs as elapsed periods; row count is evidence, not a new
acceptance threshold. Before opening the tty, fuser must confirm it is unused.
The fresh-run script still deliberately resets and sets the clock once: do not
invoke it to resume864 or while any other test owns the board. The established
unassociated/NTP-disabled preconditions remain unchanged.

Validation runs the original and patched full Python program with virtual
serial, fake images/config and a monotonic clock5% faster than wall time. ROOT
alone is redirected by AST into a temporary host workspace. No real serial or
reset tool is available in that environment. The old script labels12h PASS at
41160.62 actual simulated seconds; the corrected script observes all four
actual6h nodes and exactly four checks plus baseline. A forward wall-clock jump
is rejected by the board-duration check; busy UART refuses before opening.
The virtual read emits at60s granularity, explaining up-to60s simulated sample
lateness; this is not measured board cadence. All outputs are explicitly host
simulation evidence, not xTS PASS or a substitute for actual24h observation.
