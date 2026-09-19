# Windows iperf2 receive compatibility

1474 original300 failed at2.25seconds; Windows server timed out at2.04seconds.1467 same symptom30.6seconds. Board1469 shutdown fix separately demonstrated normal FIN1472.

Existing2.1.8 Server constructor sets SO_RCVTIMEO to half reporting interval; with -i1 this is500ms. FATALTCPREADERR treats Winsock timeout as fatal. Microsoft requires closing sockets after blocking receive timeout (https://learn.microsoft.com/en-us/windows/win32/winsock/sol-socket-socket-options). Therefore do not suppress WSAETIMEDOUT. Attempt1475 never built (compiler PATH missing); its util.h edit reverted exactly.

Candidate1476: Windows ordinary non-burst TCP receiver clears receive timeout, uses select500ms before recv, emits an empty report on select timeout, retains fatal select/recv handling and EOF. UDP/burst paths unchanged. Board original command remains300seconds; this is a separately identified host compatibility build, not stock iperf binary. Build and target review pending.
