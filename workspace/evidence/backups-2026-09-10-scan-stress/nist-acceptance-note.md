# RNG evidence and the original10-stream limitation

The original published xTS invocation uses400000 bits and10streams. Result786
remains **INCOMPLETE**, not PASS:162 numeric uniformity rows exceed0.0001,
zero starred report rows, but26 excursion/variant rows have no uniformity
value. Preserved state788 reports insufficient random-walk cycles. This was
not a workspace-allocation failure.

The unchanged `apps/testing/drivers/nist-sts/sts/src/assess.c` explains why.
For excursion tests it first retains positive P-values from eligible streams
and replaces `sampleSize` with that count. It then computes the integer
`expCount = sampleSize / 10`; when this equals0 it prints `----` instead of a
uniformity P-value. Thus1..9 eligible streams cannot produce this statistic,
even when their individual P-values pass. A10-stream invocation needs all10
streams to remain eligible for these uniformity rows.786 had only1.

The explicitly supplemental100-stream run808 finished after5372.011s:
188numeric report rows, all documented threshold checks passed. It provides
additional RNG-quality evidence after the LP RNG clock-gating fix, but changes
the stream-count parameter and therefore does not replace the original786
acceptance result. Neither the statistical algorithms nor their criteria were
edited. Missing statistics are not silently treated as PASS or N/A.

Evidence retained in this directory:

- `logs/xts786-nist-rng-clock.log`: original10-stream run and final report.
- `logs/xts788-nist-state.log`: original statistics/eligibility diagnostics.
- `logs/xts808-nist100.log`: clearly labeled100-stream supplemental result.
- `xts-current-status.md`: current acceptance status and clock-fix provenance.

Do not repeatedly rerun unchanged10-stream trials merely to select a more
favorable report. Preserve this limitation for review alongside the completed
supplemental evidence; any different acceptance interpretation must remain
explicit rather than being invented by the transport/parser.
