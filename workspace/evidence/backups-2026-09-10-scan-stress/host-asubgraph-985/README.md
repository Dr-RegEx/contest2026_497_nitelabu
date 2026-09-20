# 985 asubgraph necessary host regression

Host-only ASan/UBSan check, no UART, firmware build, or board execution.

`extract.py` takes the current actual SubGraphInstance/SubGraphFormats/SubGraphPriv,
opt_array_pcount, asubgraph_drain_mix and asubgraph_output_frame bodies verbatim. Old layout and assignment
variants are mechanically derived only for showing the regressions. Host shells
stand in for FFmpeg opaque context/frame objects; array layout and tested functions
remain actual source. The array bounds predicate is copied from av_opt_get_array,
not a full AVOption integration test. No claim of full media playback.

Checks: old array count aliases graph_desc; new count is 1; clearing array count
preserves graph_desc; one map entry is readable under the actual bounds rule.
Actual drain handles EAGAIN, EOF, read EIO and frame submission EIO. Old assignment
masks all negatives; patched function preserves EOF/errors and normalizes EAGAIN.
The actual caller also checks that drain EIO stops before EOS submission and processing,
EOS submission EIO stops before processing, and EAGAIN/EOF drain followed by
successful/EOF EOS submission continues into the mocked processing boundary.
This proves the corrected helper-to-caller error boundary, not full playback
or error delivery to the external application.

Run from workspace:

```
python3 backups/2026-09-10-scan-stress/host-asubgraph-985/extract.py
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wno-parentheses backups/2026-09-10-scan-stress/host-asubgraph-985/check.c -o /tmp/host-asubgraph-985
/tmp/host-asubgraph-985
```

Result: all checks PASS, sanitizers reported no errors. No product source modified by this audit.
