# Candidate1057: ordered empty FINAL compatibility

Independent profile xts-flat-audio-final inherits1054 continuous44.1k transport.
Frozen1054 remains unchanged. This is BUILD ONLY; user listening and board
recording validation are still required. Original1038/1049 noise is not PASS.

The original -a3 test records whole4096-byte buffers. At playback EOF it submits
an empty AUDIO_APB_FINAL buffer. The previous transport rejected bytes==0,
which left the codec repeatedly attempting its pending EOF marker.1057 accepts
only zero-length TX FINAL as a normal request, appended at the queue tail. It
has no DMA payload. Existing worker retirement only removes completed requests
from the head, so this marker cannot complete before earlier DMA data. It
receives the normal callback once, outside the mutex, with normal reference
release. Zero-length RX and non-FINAL TX remain invalid. Partial ring payloads
remain rejected; there is no padding, sample dropping or original-test change.

A bounded host check extracts the actual enqueue and worker retirement code.
ASan/UBSan verifies pending data and partial DMA completion block the final
marker, full completion returns data then final exactly once, a standalone
marker is deferred to the worker, callbacks occur outside the lock, references
balance, and invalid zero-length requests remain rejected. Leaf hardware is
not emulated; this does not prove on-board playback or clean recording.

Clean firmware build, F0 lock, whitespace and helper shell syntax passed.
Use build1057-flat-audio-final.sha256 and S31_FLAT_PROFILE=xts-flat-audio-final.
No UART/reset/flash/provisioning action occurred during this preparation.
