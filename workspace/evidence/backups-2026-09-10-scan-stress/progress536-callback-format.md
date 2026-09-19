# Callback-format evidence529–536

NuttX HEAD cc3da0ce101, Apps ae0dfd89c, no new production commit. Existing
temporary diagnostics extended with bounded SNAP pattern discovery at
offsets0..64. No payload retained/logged. This is not a full802.11 decoder.

host529 first run failed because sandbox ptrace prevents LeakSanitizer;
native escalated rerun and final extended test passed ASan+UBSan+LSan.
Cases: offsets0/24/26/32/64,65 excluded, null/truncation, invalid protocol/op,
callback statuses. Scoped new-line style and diff-check PASS.
build530 profile demo-rmt-tcpdiag via D/build-demo.sh with absolute
D/build530-tx-shape.sha256: exit0, first real build error:none.
flash531 interrupted while waiting, then original session26413 polled and
confirmed exit0/hash verified. NOT reflashed on resumption.
network532 cancelled at getpass before serial open, no network result.
LED533 exit0 and explicit user optical PASS; rgb533-optical-acceptance.md.

network534 single cold peer test, protocol7, TCP proc diagnostics,
S31_TCP_ARP_MODE=cold, AP60:ce:41:ab:02:d0, same network-probe.py tcp-diag
command template. Exit1: DHCP/gateway PASS, TCP connect timeout.
SNAP patterns present in all callback frames. Initial offset26; later34.
Before TCP: TXDONE14/fail0,TXSHAPE snap14/arp0,offset34,len108.
After TCP: TXDONE15/fail0,TXSHAPE snap15/arp0,offset34,len56.
Peer ARP submission also increased by one. Runtime establishes recognizable
SNAP at these offsets, but NOT the complete callback frame/length ABI.

Important: old discovery required36 bytes after SNAP (8LLC+28ARP), but only
read16 (8LLC+8fixedARP prefix). len56-offset34 leaves22, insufficient for
full address-bearing ARP header but sufficient for fixed-prefix recognition.
Do not read past supplied len or infer missing address fields. This was a
diagnostic classification limitation, not a demonstrated driver RX/TX bug.

host535 now verifies fixed-prefix bounds. Changed threshold to16 and output
label to arphdr, avoiding claim of complete ARP validation. ASan/UBSan/LSan
PASS. build536 profile demo-rmt-tcpdiag, receipt build536-arp-prefix.sha256,
full log build536-arp-prefix.log; exit0, first real build error:none.
flash537 exit0, only0x2000/0x200000. network538 one cold-ARP round exit1:
DHCP/gateway PASS, TCP connect timeout. Pre/post TCP total callbacks14->15,
SNAP14->15, arphdr2->3, arpfail0->0, last offset34/len56. ARP TX submission
2->3, peer request0->1. This correlates a submitted peer request with one
successful ARP-prefix completion. No complete callback address fields read;
no claim of PC receipt, full frame validity or on-air capture. RX peer reply
did not increase. Broad Wi-Fi/HE still incomplete.

firmware530/diagnostics530 and firmware536/diagnostics536 archives preserve
both variants. Board README now records user-confirmed RGB sequence and
three RMT error-path host tests, commit39be74d0093; other6 diagnostic files
remain uncommitted. These changes do not claim full RMT or xTS acceptance.
build539 profile demo-rmt, receipt build539-restore-demo.sha256, exit0,
first real error:none. flash540 restore exit0. Demo541 exit0,PID12 at
192.168.1.60:8080; HTTP542 exit0,10 samples plus complete boundary suite PASS.
Board remains on539 with Demo running. No active build/flash/serial/HTTP
session from this group. checkpoint542 archives this group after528.

Next evidence boundary: ARP fixed prefix is present in TX-done callbacks,
but full callback length/layout still not fully decoded. The cold request
gets a successful driver completion and no observed response. Before
blaming PC/TUN/AP, verify complete outgoing source MAC/IP, target MAC/IP
and broadcast Ethernet destination against live interface values; current
ARP diagnostics check selected fields/source agreement, not every value.
arp_format.c statically sets target MAC zero, broadcast destination and
copies live source fields. esp_wifi_sta_send_data forwards original pbuf/len
directly to esp_wifi_internal_tx with no additional length subtraction.
Windows UAC capture request remains unanswered; no admin capture or network
configuration change authorized by the RGB observation reply.
