# UDP queue stall localized

10s diagnostic only, notxTSPASS. 8captured waits returnETIMEDOUT(-110), queue14980bytes, limit16384, timeout500ms. No pacing>1s marker. Hostonly2packets and mainqueue10packets remain untilclose.

1403 sourcefix: devif_poll_connections previously cleared d_polltype AFTERcallbacks. sendto_next_transfer can setUDP_POLL duringcallback for nextqueuedpacket, thenouterclear discardsit. Consume priornotification BEFOREcallback and restoreifdriverstops. Sourcepath matches trace; runtimeconfirmationpending.
