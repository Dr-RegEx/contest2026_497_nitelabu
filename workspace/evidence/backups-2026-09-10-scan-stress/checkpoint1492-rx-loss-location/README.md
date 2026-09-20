# 1492 receive-loss diagnostic

Host27648 UDP datagrams, board driver RX0x2367=9063 frames (includes non-UDP), driver queue drops0x160=352, UDP received0x232e=9006 and UDP drops0. Counters are snapshots after short test; not all trailing host data necessarily observed. Difference is much larger than OS queue drops. Investigate lower Wi-Fi ingress/AP behavior before increasing OS IOBs. RX/TX byte fields render incorrectly; not used as evidence. No performance PASS.

Locked IDF components/esp_wifi/Kconfig recommends RX BA window9~12 for iperf and static RX buffers >= BA window. Candidate1493 tries BA12/static16, compared with baseline6/10; dynamicRX32, IOB96 and original40M offered load unchanged. This is a diagnostic configuration, no promised improvement.
