# Filesystem execution preparation972

Host preparation only. One original selected case, unchanged workload, fresh
short directory on already mounted scratch LittleFS. No board access, reset,
format or new PASS. Syntax/help and bounded host checks verified that reduced
iteration evidence and an error followed by PASS are rejected. Target tests
remain pending until longrun864 releases the UART.

Supported5.1.5/6/8/9/10. The source5.1.10 workload writes and reads approximately
156.25MiB across five files even though its live storage fits about160KiB; do
not assume it completes instantly. 5.1.3 is deferred due to heap-derived file
size exceeding1MiB. 5.1.7 needs free-capacity and performance review, and is not
included in this PASS runner. See fs-short-target-sequence.md for commands.
