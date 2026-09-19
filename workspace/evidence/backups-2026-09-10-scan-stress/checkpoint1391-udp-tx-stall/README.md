# Original UDP TX1391 completed with stalled traffic

-t300 -u -b40M. Board301.05s17.2KiB469bit/s13datagrams; Windows receiver reports2datagrams,2.87KiB. Returns shell after319.05s, then ps/free/ifconfig responsive and noiperfprocesses. NoPHYpagefault seen thisrun. Performance NOTPASS. Raw EXECUTION_COMPLETE_RATE_PENDING is execution only.1392 fixes separately identified TCP/UDP notification unlock-to-wait race; causality for this stall still unproven.
