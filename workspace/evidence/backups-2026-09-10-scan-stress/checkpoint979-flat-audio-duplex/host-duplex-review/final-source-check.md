# Final 979 source check

The final driver SHA256 is
`d4d35b6ff2aa4ac35ba827c46e0ef4ad1e083a3a67b95469058d30b6948999c1`,
identical to the source-manifest.json used for the completed sanitizer run.
The request/endpoint/descriptor structs and dup_stop_dma, dup_cancel,
dup_service extracted bodies were compared byte for byte: identical.
No repeat test was needed or performed.

The current enqueue code sums each preceding request's bytes minus completed
under g_lock and sets queue_timeout to
1000 + ceil(ahead * 1000 / 192000) milliseconds (converted to ticks).
The worker uses this per-request queue_timeout before service starts.
For the eighth 32768-byte request, seven full preceding requests require
1194.667 milliseconds; its queue budget is now 2195 milliseconds, rather than
1000. This removes the previously identified false queue timeout for normal
8-by-32768 traffic. Active-transfer timeout remains separately accounted.

This is a source review, not hardware timing or xTS evidence.
