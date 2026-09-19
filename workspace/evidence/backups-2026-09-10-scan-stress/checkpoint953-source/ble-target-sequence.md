# BLE target sequence — pending hardware execution

Do not open UART or flash until longrun 864 and its timing audit are complete.
Candidate 928 is controller-only build evidence. Use a successfully built,
receipted xts-flat-bttool candidate for the original tool, not 925/928 as a
substitute for the host stack. This profile disables Wi-Fi and does not qualify
coexistence. All OSAL allocations use internal SRAM; kernel and host are FLAT.

## First board-only case: original source line 4968

Confirm /dev/ttyHCI0 exists. Prepare /data/misc/bt on an explicitly selected
filesystem before starting the service; the original Unqlite backend opens
/data/misc/bt/bt_storage.db. A temporary filesystem is acceptable only for
initial enable/scan bring-up, with no persistence acceptance claim. Do not
implicitly format the persistent /apps partition or scratch Flash.

Run the original interactive application:

```text
bttool
enable
state
disable
state
```

Retain the complete boot/HCI/service log. Original expected adapter callback
states are 2 after enable and 0 after disable; state commands must report the
same values. An NSH exit code alone is insufficient. Repeated enable is a
lifecycle regression only, not an additional invented xTS case.

## Cases requiring observable peers

Source line 4990: advertising intervals 32, 160, 1600 and 16384 units using
`adv start -i <interval> -n vela-adv-test -m legacy`. Use the returned advertising
identifier/handle for stopping, following this checkout's help. Original
acceptance needs phone nRF observation and interval error no more than 5%;
a local start callback alone cannot pass this case.

Source lines 5446/5474/5502: scan PHY, mode and combined settings. Retain actual
peer reports as well as start/stop results. The original document explicitly
leaves 2M scanning subject to development confirmation; preserve that ambiguity
rather than accepting an unsupported parameter as success.

Multi-advertiser cases require matching controller/host advertising-set counts
and peer observation. Discovery time/success-rate, five-peer discovery,
pairing success-rate and scan-efficiency cases need their original repetition
counts and real peers. They remain separate from a successful enable case.

Wi-Fi/BLE coexistence requires a separate supported combined profile and both
radio workloads; it is not covered by this BLE-only candidate.
