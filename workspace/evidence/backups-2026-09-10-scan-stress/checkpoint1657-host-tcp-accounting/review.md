# Windows TCP sender accounting diagnostic

A loopback TCP receiver with a 4096-byte receive buffer drained at 700ms intervals. Both clients used -n 262144 -w 8k -i 1 -C. Stock iperf2 reported 256KiB but delivered 1703936 bytes; the1656 candidate reported and delivered262144 bytes. Both socket streams reached EOF before the90s observation bound. This is a host-only fixed-byte diagnostic, not an original xTS result. PowerShell Process.ExitCode serialized null, so no exit-code claim is made; complete client summaries and receiver EOF/byte counts are the evidence.

Candidate retains1641 receive compatibility fixes and clears SO_SNDTIMEO only for ordinary Windows RunTCP. A blocking write can extend a timed interval while draining; a300s original test must still be reviewed for endpoint totals and actual elapsed time. No target firmware change for this fix. Board1646 discrepancy is consistent with this demonstrated host fault, but a target fixed-byte comparison remains necessary.

Microsoft documents that a timed-out blocking send leaves the connection indeterminate and it should be closed: https://learn.microsoft.com/en-us/windows/win32/winsock/sol-socket-socket-options . The stock tool retries WSAETIMEDOUT as zero bytes. This reproduction supports replacing that behavior for ordinary Windows TCP measurement.
