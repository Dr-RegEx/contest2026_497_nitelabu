# 1144 KV capacity candidate, not an established root-cause fix

Preserves original KV stability10 workload, same UnQLite backend and five-second commit interval, and1130 error-only diagnostics. Only board test-volume selector changes to the existing3MiB d00000..1000000 volume. The1MiB c00000..d00000 failure volumes remain backed up and untouched by this candidate.

Actual1119 failed with IO errors on1MiB despite removal of old payloads.1139 was deliberately interrupted at fourth-round start for user audio retake, without observedIOerrors; its persisted backup1140 mounts read-only as106/256 blocks and307200B database. This does not measure peak live copy-on-write allocation, prove root cause, or pass stability.1144 is a bounded capacity candidate for the same full workload, not an acceptance claim.

No target reset/format/write during preparation. The existing3MiB volume contains preserved filesystem evidence. Before target use retain a current volume backup, mount without formatting, check no conflicting processes and actual free capacity, then perform original DB-removal/reboot precondition if that DB exists. No unrelated files may be deleted for a passing result. Pending audio retake keeps the board on1131 until user signals开始.

Mount helper mount1144-kv-large-existing.py is exact1144 receipt-guarded, asserts XTS_FLASH_LARGE and d00000/300000 boot marker, and never formats. run1144-kv-large-stability.py requires explicit fresh mount evidence and previous DB-absent/removal evidence before the required reboot; original10 rounds unchanged, diagnostic/commit failures rejected. Only syntax/help/offline image preflight performed; UART never opened during these checks. Supplemental frozen runner hashes:checkpoint1144-kv-large/SHA256SUMS-runners.
