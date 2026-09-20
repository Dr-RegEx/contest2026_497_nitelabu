# Resume at506

NuttX codex/esp32s31-port HEAD21d0fd73f0c; Apps ae0dfd89c.
New committed groups:532aa75837e write contract,8a2a8f8f019 initial
semaphore failure,21d0fd73f0c lower-half initialization allocation cleanup.
All host regressions and respective builds/flash/40-frame LED passed;
optical remains unverified, user remote and reminded before each RGB run.

Latest sealed checkpoint505: progress492-505.tar.gz and SHA256SUMS-progress505,
verified exit0. Contains503 firmware plus commit bundle based on532a, which
is in prior sealed491. Six existing network/monitor diagnostic edits remain
uncommitted; no new RMT source/test WIP. Reference and Apps unchanged.

Board runs503; demo506 restarted successfully after LED505, exit0, PID12,
http://192.168.1.60:8080/. Leaves authorized Wi-Fi connected, no active serial
or build process from this group. Latest HTTP100+boundaries PASS was500 on
494 before the two initialization-failure free calls in503. Do not claim
HTTP500 used503. demo506 log is outside sealed505 (created afterward).

Next RMT review found rmt_driver_install ignores circbuf_init errors and
returns OK even if rmt_isr_register fails; publishes p_rmt_obj before failure
and lacks complete cleanup. No fix started for this deeper group.
CONFIG_SPIRAM_USE_MALLOC has no definition in NuttX Kconfig search, but
legacy conditional branches use mixed calloc/kmm allocators and omit
semaphore init. Do not remove/normalize blindly; tests must cover intended
allocation/error variants. rmt_isr_register itself tears down CPU IRQ on
irq_attach failure. Need actual-function fault injection for allocation,
circular buffer, IRQ and retry, not just happy-path LED.
Correct circular-buffer implementation path: libs/libc/misc/lib_circbuf.c.

RMT active-TX cancellation/refill/multiwriter issues are in rmt-followup494.md.
Wi-Fi unresolved ARP/HE evidence and pending optional Windows UAC capture
authorization unchanged. No user answer, no capture/filter modifications.
Do not repeat request or initiate UAC without response. Other board work
can continue while capture is pending. Overall goal still active, not complete.
