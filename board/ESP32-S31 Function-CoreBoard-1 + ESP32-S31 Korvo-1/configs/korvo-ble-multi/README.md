# Korvo concurrent Bluetooth advertising

This single-core FLAT profile extends `korvo-ble-gatt` with two host connection
objects. The Bluetooth host reserves one object for each active connectable
advertisement, even before a remote device connects. Two advertising slots
with only one connection object cannot start two connectable instances.

The profile enables privacy and two advertising instances. Use temporary
`/data` storage before starting `bttool`; it does not provision Wi-Fi or
preserve pairing information across a reset. Extended static-address
advertising, SMP coexistence, LE Audio and persistent bonding require their
own validation.
